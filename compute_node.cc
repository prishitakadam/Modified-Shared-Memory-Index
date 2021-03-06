#include <iostream>
#include "rdma.h"
//global values for the test
bool test_start = false;
size_t thread_ready_num = 0;
size_t thread_finish_num = 0;
size_t thread_num;
std::mutex startmtx;
std::mutex finishmtx;
std::condition_variable cv;


void
client_thread(RDMA_Manager *rdma_manager, int iteration, ibv_mr **local_mr_pointer, ibv_mr **remote_mr, size_t msg_size,
              int r_w) {
    std::unique_lock<std::mutex> lck_start(startmtx);
    rdma_manager->RDMA_Write(remote_mr[0], local_mr_pointer[0],
                             msg_size, "", IBV_SEND_SIGNALED, 1);
    thread_ready_num++;
    if (thread_ready_num >= thread_num) {
        cv.notify_all();
    }
    while (!test_start) {
        cv.wait(lck_start);

    }

    lck_start.unlock();
    for (int i = 0; i < iteration; i++) {
        if ((i + 1) % 10 == 0) {
            int j = (i + 1) % 10;
            if (r_w)
                rdma_manager->RDMA_Write(remote_mr[j], local_mr_pointer[j],
                                         msg_size, "", IBV_SEND_SIGNALED, 10);
            else
                rdma_manager->RDMA_Read(remote_mr[j], local_mr_pointer[j],
                                        msg_size, "", IBV_SEND_SIGNALED, 10);
        } else {
            if (r_w)
                rdma_manager->RDMA_Write(remote_mr[0], local_mr_pointer[0],
                                         msg_size, "", IBV_SEND_SIGNALED, 0);
            else
                rdma_manager->RDMA_Read(remote_mr[0]    , local_mr_pointer[0],
                                        msg_size, "", IBV_SEND_SIGNALED, 0);
        }

    }
    std::unique_lock<std::mutex> lck_end(startmtx);
    thread_finish_num++;
    if (thread_finish_num >= thread_num) {
        cv.notify_all();
    }
    lck_end.unlock();
}
int main()
{
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



    printf("multiple threaded demo");
    size_t read_block_size;
//    std::cout << "block size:\r" << std::endl;
//    std::cin >> read_block_size;
    read_block_size = 1048576;
    //  table_size = read_block_size+64;
    int readorwrite;
    std::cout << "Read or write:\r" << std::endl;//read 0 write anything else
    std::cin >> readorwrite;

    std::cout << "thread num:\r" << std::endl;
    std::cin >> thread_num;
    rdma_manager->Mempool_initialize(std::string("test"), read_block_size);
    ibv_mr* RDMA_local_chunks[thread_num][10];
    ibv_mr* RDMA_remote_chunks[thread_num][10];
    int iteration = 100;
    long int starts;
    long int ends;
    std::thread* thread_object[thread_num];
    for (size_t i = 0; i < thread_num; i++){
    //        SST_Metadata* sst_meta;

        for(size_t j= 0; j< 1; j++){
            rdma_manager->Allocate_Remote_RDMA_Slot(RDMA_remote_chunks[i][j]);

            rdma_manager->Allocate_Local_RDMA_Slot(RDMA_local_chunks[i][j], std::string("test"));
            size_t msg_size = read_block_size;
            memset(RDMA_local_chunks[i][j]->addr,1,msg_size);
        }

    }
    for(size_t i = 0; i < thread_num; i++)
    {


        thread_object[i] = new std::thread(client_thread, rdma_manager, iteration,
                                           RDMA_local_chunks[i], RDMA_remote_chunks[i], read_block_size, readorwrite);
        thread_object[i]->detach();

    }
    std::unique_lock<std::mutex> l_s(startmtx);
    while (thread_ready_num!= thread_num){
        cv.wait(l_s);
    }
    test_start = true;
    cv.notify_all();
    starts = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    l_s.unlock();
    std::unique_lock<std::mutex> l_e(finishmtx);

    while (thread_finish_num < thread_num) {
        cv.wait(l_e);
    }
    ends  = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    printf("thread has finished.\n");
    l_e.unlock();
    double bandwidth = ((double)read_block_size*thread_num*iteration) / (ends-starts) * 1000;
    double latency = ((double) (ends-starts)) / (thread_num * iteration);
    std::cout << (ends-starts) << std::endl;
    std::cout << "Size: " << read_block_size << "Bandwidth is " << bandwidth << "MB/s" << std::endl;
    std::cout << "Size: " << read_block_size << "Dummy latency is " << latency << "ns" << std::endl;


    return 0;
}