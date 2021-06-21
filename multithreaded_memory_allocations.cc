#include <iostream>
#include "rdma.h"

size_t thread_num;

bool mem_test_start = false;
size_t mem_thread_ready_num = 0;
size_t mem_thread_finish_num = 0;
size_t mem_thread_num;
std::mutex mem_startmtx;
std::mutex mem_finishmtx;
std::condition_variable mem_cv;

void mulithreaded_memory_allocations(RDMA_Manager *rdma_manager, size_t msg_size){
    std::unique_lock<std::mutex> mem_lck_start(mem_startmtx);
    mem_thread_ready_num++;
    if (mem_thread_ready_num >= mem_thread_num) {
        mem_cv.notify_all();
    }
    while (!test_start) {
        mem_cv.wait(mem_lck_start);

    }

    mem_lck_start.unlock();

    for (size_t i = 0; i < thread_num; i++){
        for(size_t j= 0; j< 1; j++){
            rdma_manager->Allocate_Remote_RDMA_Slot(RDMA_remote_chunks[i][j]);

            rdma_manager->Allocate_Local_RDMA_Slot(RDMA_local_chunks[i][j], std::string("test"));
            // size_t msg_size = read_block_size;
            memset(RDMA_local_chunks[i][j]->addr,1,msg_size);
        }

    }

    std::unique_lock<std::mutex> mem_lck_end(startmtx);
    mem_thread_finish_num++;
    if(mem_thread_finish_num >= mem_thread_num) {
        mem_cv.notify_all();
    }
    mem_lck_end.unlock();
}

int main(){
    truct config_t config = {
      NULL,  /* dev_name */
      NULL,  /* server_name */
      19875, /* tcp_port */
      1,	 /* ib_port */ //physical
      1, /* gid_idx */
      4*10*1024*1024 /*initial local buffer size*/
  };
  size_t remote_block_size = 1024*1024;
  //Initialize the rdma manager, the remote block size will be configured in the beggining.
  // remote block size will always be the same.
  RDMA_Manager* rdma_manager = new RDMA_Manager(config, remote_block_size);
  // Unlike the remote block size, the local block size is adjustable, and there could be different
  // local memory pool with different size. each size of memory pool will have an ID below is "4k"
  rdma_manager->Mempool_initialize(std::string("4k"), 4*1024);

    //client will try to connect to the remote memory, now there is only one remote memory.
  rdma_manager->Client_Set_Up_Resources();
    //below we will allocate three memory blocks, local send, local receive and remote blocks.
    ibv_mr* send_mr;
    ibv_mr* receive_mr;
    ibv_mr* remote_mr;
    // these two lines of code will allocate block of 4096 from 4k memory pool, there are also Deallocate functions
    // which is not shown here, you are suppose to deallocate the buffer for garbage collection.
    rdma_manager->Allocate_Local_RDMA_Slot(send_mr, "4k");
    rdma_manager->Allocate_Local_RDMA_Slot(receive_mr, "4k");
    // this line of code will allocate a remote memory block from the remote side through RDMA rpc.
    rdma_manager->Allocate_Remote_RDMA_Slot(remote_mr);
    // Supposing we will send a string message
    std::string str = "RDMA MESSAGE\n";
    memcpy(send_mr->addr, str.c_str(), str.size());
    memset((char*)send_mr->addr+str.size(), 0,1);
    // for RDMA send and receive there will be five parameters, the first and second parameters are the address for the
    // local and remote buffer
    // q_id is "", then use the thread local qp.
    rdma_manager->RDMA_Write(remote_mr, send_mr, str.size()+1, "",IBV_SEND_SIGNALED, 1);
    rdma_manager->RDMA_Read(remote_mr, receive_mr, str.size()+1, "",IBV_SEND_SIGNALED, 1);
    printf((char*)receive_mr->addr);



    printf("multiple threaded demo");
    size_t read_block_size;

    //    std::cout << "block size:\r" << std::endl;
//    std::cin >> read_block_size;
    read_block_size = 1048576;
    //  table_size = read_block_size+64;
    std::cout << "No of mem slots to be allocated :\r" << std::endl;
    std::cin >> mem_thread_num;

    std::cout << "thread num:\r" << std::endl;
    std::cin >> thread_num;

    rdma_manager->Mempool_initialize(std::string("test"), read_block_size);
    std::thread* mem_thread_object[mem_thread_num];
    for(size_t i = 0; i < mem_thread_num; i++){
        mem_thread_object[i] = new std::thread(mulithreaded_memory_allocations, rdma_manager, read_block_size);
        mem_thread_object[i]->detach();
    }

    std::unique_lock<std::mutex> mem_l_s(mem_startmtx);
    while (mem_thread_ready_num!= mem_thread_num){
        mem_cv.wait(mem_l_s);
    }
    mem_test_start = true;
    mem_cv.notify_all();
    starts = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    mem_l_s.unlock();
    std::unique_lock<std::mutex> l_e(mem_finishmtx);

    while (mem_thread_finish_num < mem_thread_num) {
        mem_cv.wait(mem_l_e);
    }
    ends  = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    printf("thread has finished.\n");
    mem_l_e.unlock();
    double bandwidth = ((double)read_block_size*thread_num*iteration) / (ends-starts) * 1000;
    double latency = ((double) (ends-starts)) / (thread_num * iteration);
    std::cout << (ends-starts) << std::endl;
    std::cout << "Size: " << read_block_size << "Bandwidth is " << bandwidth << "MB/s" << std::endl;
    std::cout << "Size: " << read_block_size << "Dummy latency is " << latency << "ns" << std::endl;

    return 0;
}