#include <stdio.h>
#include <poll.h>
#include <litehook.h>
#include <pthread/pthread.h>
#include <os/log.h>

#include <xpc/xpc.h>
#include "../xpc_private.h"

int ptrace(int request, pid_t pid, caddr_t addr, int data);
#define PT_TRACE_ME     0

uintptr_t litehook_get_dsc_slide(void);
kern_return_t litehook_unprotect(vm_address_t addr, vm_size_t size);
kern_return_t litehook_protect(vm_address_t addr, vm_size_t size);

bool gAccessModified = false;

struct poll_thread_data {
	int fd;
	void *page;
	dispatch_semaphore_t sema;
};

void *poll_thread(void *arg1)
{
	struct poll_thread_data *d = arg1;

	dispatch_semaphore_signal(d->sema);
	struct pollfd pfd = { .fd = d->fd, .events = POLLIN };
	poll(&pfd, 1, -1);

	if (d->page) {
		*(volatile uint64_t *)d->page;
	}

	return NULL;
}

void get_cs_debugged(void)
{
	static dispatch_once_t onceToken;
	dispatch_once (&onceToken, ^{
		if (ptrace(PT_TRACE_ME, 0, NULL, 0) < 0) {
			perror("ptrace: start tracing");
			exit(-1);
		}
	});
}

void xpc_server(void)
{
	mach_port_t *registeredPorts;
	mach_msg_type_number_t registeredPortsCount = 0;
	mach_ports_lookup(mach_task_self(), &registeredPorts, &registeredPortsCount);

	mach_port_t serverPort = MACH_PORT_NULL;
	mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &serverPort);
	mach_port_insert_right(mach_task_self(), serverPort, serverPort, MACH_MSG_TYPE_MAKE_SEND);

	registeredPorts[2] = serverPort;

	mach_ports_register(mach_task_self(), registeredPorts, 3);

	while (true) {
		xpc_object_t xdict;
		if (!xpc_pipe_receive(serverPort, &xdict)) {
			struct poll_thread_data d = {
				.page = (void *)(xpc_dictionary_get_uint64(xdict, "pageToAccess") + litehook_get_dsc_slide()),
				.fd = xpc_dictionary_dup_fd(xdict, "comFd"),
				.sema = dispatch_semaphore_create(0),
			};

			// The poll thread needs to be running by the time we send the xpc reply
			pthread_t pollThread;
			pthread_create(&pollThread, NULL, poll_thread, (void *)&d);
			dispatch_semaphore_wait(d.sema, DISPATCH_TIME_FOREVER);

			if (gAccessModified) {
				get_cs_debugged();
				litehook_unprotect ((uint64_t)d.page, 0x4000);
				litehook_protect   ((uint64_t)d.page, 0x4000);
			}

			xpc_object_t xreply = xpc_dictionary_create_reply(xdict);
			xpc_dictionary_set_uint64(xreply, "result", 0);

			xpc_pipe_routine_reply(xreply);
			pthread_join(pollThread, NULL);

			xpc_release(xreply);
			xpc_release(xdict);
			close(d.fd);
		}
	}
}

int main(int argc, char *argv[], char *envp[]) {
	if (argc > 1) {
		if (!strcmp(argv[1], "access-original") || !strcmp(argv[1], "access-modified")) {
			gAccessModified = !strcmp(argv[1], "access-modified");
			xpc_server();
		}
	}
}
