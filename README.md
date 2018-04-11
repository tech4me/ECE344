ECE344 OS/161 (Winter 2018)
=========================
This solution is able to pass all test cases besides parallelvm 18 in asst3, which will timeout. (The required time is 60s, and this solution uses about 700s)
Overall this solution should be able to get: 178/185 (7 marks for parallelvm 18)

Issues
--------
1. Still there still might be some synchronization problem with this code. When I tired to implement swap with write if dirty the result is sometimes not consistent. Therefore the code for swap with write if dirty is not committed to this repository
2. Right now the code replaces page randomly, this should be improved with strategies such as aging.
3. There is no system thread that helps with evicting main memory in a timely manner so later page allocation will not eviction
4. As it is right now, the code will avoid evicting shared pages(created by fork with copy-on-write), however this could be disabled by changing the function coremap_page_to_evcit(). If it is disabled shared pages will be duplicated on eviction. This behavior is not ideal, data structures should be implemented to keep track of which page table element a page corresponds to even after eviction, so when it swapped back in it can still be shared by multiple process.
5. The code flush TLB very aggressively, this leads to very frequent TLB misses. TLB flushes should be avoided as much as possible, and even the pid bits in TLB should be used to help.
6. (Special optimization) because the parallelvm testcase access pages in a very orderly fashion. Some special optimization certainly could be made. Pre-loading pages from swap can be considered.

Conclusion
---------------
With all of the above things considered, I believe is it possible to achieve 185/185.
