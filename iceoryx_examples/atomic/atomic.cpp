#include "ffli.hpp"
#include <iostream>
#include <sys/mman.h>
#include <thread>

//#if defined(__ANDROID_VM_SHM__)
#include <fcntl.h> 
#include <unistd.h>  
//#endif
#include <cerrno>
#include <cstring>

#define SHM_SIZE     0x1800000

static void* base_address = nullptr;

#if defined(QNX) || defined(QNX__) || defined(__QNX__)
bool create_qnx_shm(uint64_t address)
{
	void *shm = reinterpret_cast<void *>(mmap_device_io(SHM_SIZE, address));
	if (shm == MAP_FAILED) 
	{
        std::perror("mmap_write_device_io failed");
        std::cerr << "Couldn't map 0x" << shm << " : " << std::strerror(errno) << std::endl;
		return false;
	}
	base_address = shm;
	return true;
}
#else
bool create_android_shm(std::string dev_path)
{
	auto fd = open(dev_path.c_str(),O_RDWR);
	if(fd < 0)
	{
        std::perror("open shm device failed");
        std::cerr << "Open" << dev_path << " : " << std::strerror(errno) << std::endl;
		return false;
	}
	auto shm = mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (reinterpret_cast<uintptr_t>(shm) % 8 != 0) 
	{
        std::perror("shmem is not 8-byte aligned!");
		return false;
	}
	if (shm == MAP_FAILED) 
	{
		close(fd);
        std::perror("mmap_write_device_io failed");
        std::cerr << "Couldn't map 0x" << shm << " : " << std::strerror(errno) << std::endl;
		return false;
	}
	base_address = shm;
	return true;
}
#endif

void test(MpmcLoFFLi *obj)
{
	while(true)
	{
	   MpmcLoFFLi::Index_t idx;
       bool ret = obj->pop(idx);
	   if(ret)
	   {
	       obj->push(idx);
	   }
	}

}

int main(int args, char** argv)
{
	bool ret = false;
#if defined(QNX) || defined(QNX__) || defined(__QNX__)
    uint64_t address = strtoull(argv[1], nullptr, 16);
    ret = create_qnx_shm(address);
#else
    ret = create_android_shm(argv[1]);
#endif

	MpmcLoFFLi *pfll_manager = nullptr;
	if (strcmp(argv[3], "1") == 0) {
	    pfll_manager = new(base_address) MpmcLoFFLi();
	}
	else
	{
	    pfll_manager = reinterpret_cast<MpmcLoFFLi*>(base_address);
	}
	MpmcLoFFLi::Index_t *pfll = (MpmcLoFFLi::Index_t*)((char*)base_address + sizeof(MpmcLoFFLi) + 100);
	if(!pfll_manager)
	{
	    std::cout << "pfil_manager is null" << std::endl;
	}

	if (strcmp(argv[3], "1") == 0) {
	    pfll_manager->init(pfll,5,true);
	}
	else
	{
	    pfll_manager->init(pfll,5,false);
	}

	if(args == 4)
	{

	    std::thread* threads[100];
	    uint32_t thread_num = strtoul(argv[2],nullptr,10);
	    std::cout << "test via " << thread_num << " threads!" << std::endl;

	    for (int i = 0; i < thread_num; ++i) {
	    	threads[i] = new std::thread(test, pfll_manager);
	    }

	    for (int i = 0; i < thread_num; ++i) {
            threads[i]->join();
            delete threads[i];
        }
    }
	else
	{
		while(true)
		{
		    pfll_manager->printInfo();
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}

}