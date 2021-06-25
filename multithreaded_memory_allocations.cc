#include <iostream>
#include "rdma.h"

size_t thread_num;
size_t j_size = 2000;
bool mem_test_start = false;
size_t mem_thread_ready_num = 0;
size_t mem_thread_finish_num = 0;
// size_t mem_thread_num;
std::mutex mem_startmtx;
std::mutex mem_finishmtx;
std::condition_variable mem_cv;

void mulithreaded_memory_allocations(RDMA_Manager *rdma_manager, ibv_mr **local_chunks, ibv_mr **remote_chunks, size_t i, size_t msg_size){
    std::unique_lock<std::mutex> mem_lck_start(mem_startmtx);
    mem_thread_ready_num++;
    std::cout<<"Created "<<mem_thread_ready_num<< "\n";
    if (mem_thread_ready_num >= thread_num) {
        mem_cv.notify_all();
    }
    while (!mem_test_start) {
        mem_cv.wait(mem_lck_start);

    }

    mem_lck_start.unlock();

    //Allocate in main function
    
    for(size_t j= 0; j< j_size; j++){//j should be bigger value
        rdma_manager->Allocate_Remote_RDMA_Slot(remote_chunks[j]);

        rdma_manager->Allocate_Local_RDMA_Slot(local_chunks[j], std::string("test"));
        // size_t msg_size = read_block_size;
        memset(local_chunks[j]->addr,1,msg_size);
    }


    std::unique_lock<std::mutex> mem_lck_end(mem_startmtx);
    mem_thread_finish_num++;
    std::cout<<"Finished "<<mem_thread_finish_num<<" \n";
    if(mem_thread_finish_num >= thread_num) {
        mem_cv.notify_all();
    }
    mem_lck_end.unlock();
}

int main(){
    struct config_t config = {
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



    printf("Multiple Threaded Demo\n");
    size_t read_block_size;

    //    std::cout << "block size:\r" << std::endl;
//    std::cin >> read_block_size;
    read_block_size = 1048576;
    //  table_size = read_block_size+64;

    std::cout << "Thread Num:\r" << std::endl;
    std::cin >> thread_num;

    // std::cout << "\nJ Size : \r" << std::endl;
    // std::cin >> j_size;


    rdma_manager->Mempool_initialize(std::string("test"), read_block_size);
    // ibv_mr* RDMA_local_chunks[thread_num][10];
    // ibv_mr* RDMA_remote_chunks[thread_num][10];
    
    long int starts;
    long int ends;
    int iteration = 100;
    std::thread* mem_thread_object[thread_num];
    for(size_t i = 0; i < thread_num; i++){
        mem_thread_object[i] = new std::thread(mulithreaded_memory_allocations, rdma_manager, RDMA_local_chunks[i], RDMA_remote_chunks[i], i, read_block_size);
        mem_thread_object[i]->detach();
    }

    std::unique_lock<std::mutex> mem_l_s(mem_startmtx);
    while (mem_thread_ready_num!= thread_num){
        mem_cv.wait(mem_l_s);
    }
    mem_test_start = true;
    mem_cv.notify_all();
    starts = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    mem_l_s.unlock();
    std::unique_lock<std::mutex> mem_l_e(mem_finishmtx);

    while (mem_thread_finish_num < thread_num) {
        mem_cv.wait(mem_l_e);
    }
    ends  = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    printf("Thread has finished \n");
    mem_l_e.unlock();
    double throughput = ((double)j_size*thread_num)/(ends-starts);
    // double bandwidth = ((double)read_block_size*thread_num*iteration) / (ends-starts) * 1000;
    // double latency = ((double) (ends-starts)) / (thread_num * iteration);
    std::cout << "Throughput is " << throughput << "M/s" << std::endl;
    // std::cout << "Size: " << read_block_size << "Bandwidth is " << bandwidth << "MB/s" << std::endl;
    // std::cout << "Size: " << read_block_size << "Dummy latency is " << latency << "ns" << std::endl;

    return 0;
}