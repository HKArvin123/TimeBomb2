<img src="Icon.png" width="64" />

# TimeBomb 2

A TrollStore app that can trigger the infamous spinlock panic on iOS 15 arm64e stock iOS, proving it's not an issue with a jailbreak.

## Spinlock Panics: A brief summary

When a process in iOS starts, the system will map the shared region into it, for which the same page table will be shared among every process. When some process makes a local modification to a page contained within the shared region (e.g. when using MSHookFunction), the system will reallocate the level 3 page table and replace the shared level 3 page table with it, then it will modify the entries as requested. On iOS 15, there is a bug within the PMAP_CS code in the Kernel where it will still take the pmap lock of the shared region pmap, even when the page to be faulted in is contained within the reallocated page table. This can cause a race condition if the following events happen exactly within this order:

- Page fault triggered on thread A (modified page table)
- Page fault triggered on thread B (original page table, tries to fault in the same physical page as thread A)
- Thread A takes process pmap lock in `pmap_enter_options`
- Thread A takes page spinlock in `pmap_enter_options`
- Thread B takes shared region pmap lock in `pmap_enter_options`
- Thread A takes shared region pmap lock in `pmap_cs_enforce`
- Thread B takes page spinlock in `pmap_enter_options`

Since thread A in this case is waiting on thread B, but thread A has previously aquired the page spinlock and not released it, thread B trying to acquire the same page spinlock will result in a deadlock, but since it's a spinlock, it will time out instead and the system will panic.

This bug will randomly happen on systems where some shared regions pages are hooked, which is why normally only jailbroken devices are affected by this. But as this app demonstrates, it can also happen on stock devices if some process is being debugged. The app will spawn 10 processes that use the original page tables and 10 processes that use modified / reallocated page tables, then it enumerates all `__TEXT` pages and causes all 20 processes to fault them in at the same time. As soon as it hits a page that was previously paged out of the shared region, the issue will be triggered.

Since `pmap_cs_enforce` is only called on pages that have a code signature, this issue only affects code / `__TEXT` pages. Additionally non-arm64e devices do not use PMAP_CS for code signature verification, which is why they don't suffer from the bug. In iOS 16, this race condition was fixed inside `pmap_cs_enforce` by making sure that pmap lock is not already taken and gracefully retrying later if it is.