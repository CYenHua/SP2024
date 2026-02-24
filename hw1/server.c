#include "server.h"

const unsigned char IAC_IP[3] = "\xff\xf4";
const char* file_prefix = "./csie_trains/train_";
const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";
const char* welcome_banner = "======================================\n"
                             " Welcome to CSIE Train Booking System \n"
                             "======================================\n";

const char* lock_msg = ">>> Locked.\n";
const char* exit_msg = ">>> Client exit.\n";
const char* cancel_msg = ">>> You cancel the seat.\n";
const char* full_msg = ">>> The shift is fully booked.\n";
const char* seat_booked_msg = ">>> The seat is booked.\n";
const char* no_seat_msg = ">>> No seat to pay.\n";
const char* book_succ_msg = ">>> Your train booking is successful.\n";
const char* invalid_op_msg = ">>> Invalid operation.\n";

#ifdef READ_SERVER
char* read_shift_msg = "Please select the shift you want to check [902001-902005]: ";
#elif defined WRITE_SERVER
char* write_shift_msg = "Please select the shift you want to book [902001-902005]: ";
char* write_seat_msg = "Select the seat [1-40] or type \"pay\" to confirm: ";
char* write_seat_or_exit_msg = "Type \"seat\" to continue or \"exit\" to quit [seat/exit]: ";
#endif



static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

int accept_conn(void);
// accept connection

static void getfilepath(char* filepath, int extension);
// get record filepath






void unlock(request* reqP){
    
    int i = reqP->booking_info.shift_id - 902001;
    if(reqP->booking_info.shift_id == -1) return;
    for(int j = 0; j < 40; j++){
        if(svr.seat_lock[i][j] == reqP->conn_fd){
            svr.seat_lock[i][j] = -1;
            struct flock lock;
            lock.l_type = F_UNLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = j * 2;
            lock.l_len = 1;
            lock.l_pid = 0;
            fcntl(trains[i].file_fd, F_SETLK, &lock);
        }
    }
    
}
bool CheckAllOne(request* reqP){
    int train = atoi(reqP->buf);
    char buf[85];
    lseek(trains[train - 902001].file_fd, 0, SEEK_SET);
    ssize_t bytes_read = read(trains[train - 902001].file_fd, buf, 80);
    for(int i = 0; i < 80; i+=2){
        if(buf[i] == '0'){
            return false;
        }
    }
    return true;
}

int CheckBooked(request* reqP){
    int seat = atoi(reqP->buf) - 1;
    seat *= 2;
    char buf[seat + 3];
    memset(buf, 0, sizeof(buf));
    ssize_t bytes_read;
    lseek(trains[reqP->booking_info.shift_id - 902001].file_fd, seat, SEEK_SET);
    bytes_read = read(trains[reqP->booking_info.shift_id - 902001].file_fd, buf, 1);
    if(buf[0] == '1') return 1;//booked
    return 0;
}

bool IsNumber(char *str){
    int i = 0;
    while (str[i] != '\0') {
        if (!isdigit(str[i])) { // 如果有一個字元不是數字，返回 0
            return 0;
        }
        i++;
    }
    return 1;
}

void SelectSeat(request *reqP){
    int seat = (atoi(reqP->buf) - 1);
    int train = reqP->booking_info.shift_id;
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = seat * 2;
    lock.l_len = 1;
    lock.l_pid = 0;

    if(reqP->booking_info.seat_stat[seat] == CHOSEN){
        write(reqP->conn_fd, cancel_msg, strlen(cancel_msg));  
        reqP->booking_info.seat_stat[seat] = UNKNOWN;
        reqP->booking_info.num_of_chosen_seats--;
        svr.seat_lock[reqP->booking_info.shift_id - 902001][seat] = -1;
        lock.l_type = F_UNLCK;
        fcntl(trains[train - 902001].file_fd, F_SETLK, &lock);
        return;
    }
    if(fcntl(trains[train - 902001].file_fd, F_GETLK, &lock) == -1){
        return;
    }
    if(lock.l_type != F_UNLCK || (svr.seat_lock[reqP->booking_info.shift_id - 902001][seat] != -1)){ //有鎖
        write(reqP->conn_fd, lock_msg, strlen(lock_msg));
        return;
    }
    else{
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_len = 1;
        lock.l_pid = 0;
        lock.l_start = seat * 2;
        
        reqP->booking_info.seat_stat[seat] = CHOSEN;
        reqP->booking_info.num_of_chosen_seats++;
        fcntl(trains[train - 902001].file_fd, F_SETLK, &lock);
        svr.seat_lock[reqP->booking_info.shift_id - 902001][seat] = reqP->conn_fd;
    }
    return;
}

#ifdef READ_SERVER
int print_train_info(request *reqP) {
    int i, bytes_read;
    char buf[85];
    int fd = trains[reqP->booking_info.shift_id - 902001].file_fd;
    struct flock lock;
    lock.l_whence = SEEK_SET;
    lock.l_len = 1;
    lock.l_pid = 0;
    memset(buf, 0, sizeof(buf));
    lseek(fd, 0, SEEK_SET);
    bytes_read = read(fd, buf, sizeof(buf) - 1);
    if (bytes_read < 0) {
        return -1;
    }
    for(int i = 0; i < 80; i+=2){
        struct flock lock;
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_len = 1;
        lock.l_pid = 0;
        lock.l_start = i;
        fcntl(trains[reqP->booking_info.shift_id - 902001].file_fd, F_GETLK, &lock);
        if((lock.l_type != F_UNLCK) && buf[i] == '0')  buf[i] = '2';
    }
    write(reqP->conn_fd, buf, strlen(buf));
    return 0;
}
#else
int print_train_info(request *reqP) {
    char buf[MAX_MSG_LEN]; //buf用來儲存格式化後的訂位資訊
    char chosen_seat[200]; //選擇的座位
    char paid[200]; //已支付的座位
    int chosen = 0, pay = 0;
    chosen_seat[0] = '\0';

    memset(buf, 0, sizeof(buf)); 
    memset(chosen_seat, 0, sizeof(chosen_seat));
    memset(paid, 0, sizeof(paid));
    //將格式化的字串寫入到 buf 中
    for(int i = 0; i < SEAT_NUM; i++){
        if(reqP->booking_info.seat_stat[i] == CHOSEN){
            if (chosen > 0) {
                sprintf(chosen_seat + strlen(chosen_seat), ","); 
            }  
            sprintf(chosen_seat + strlen(chosen_seat), "%d", i+1);
        chosen++;
        }
        else if(reqP->booking_info.seat_stat[i] == PAID){
            if (pay > 0) {
                sprintf(paid + strlen(paid), ",");
            }
            sprintf(paid + strlen(paid), "%d", i+1);
            pay++;
        }
    }
    sprintf(buf, "\nBooking info\n"
                 "|- Shift ID: %d\n"
                 "|- Chose seat(s): %s\n"
                 "|- Paid: %s\n\n"
                 ,reqP->booking_info.shift_id, chosen_seat, paid);
    write(reqP->conn_fd, buf, strlen(buf));
    return 0;
}
#endif


int handle_read(request* reqP) {
    int r;
    char buf[MAX_MSG_LEN]; //存放讀取到的數據
    size_t len; //有效數據的長度
    //fprintf(stderr, "%zu\n", reqP->buf_len);

    memset(buf, 0, sizeof(buf)); //將 buf 數組初始化為全0，確保不會有殘留數據。
    //memset(reqP->buf, 0,  sizeof(reqP->buf));

    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf)); //從client讀取數據到 buf 中，讀取的最大字節數為 sizeof(buf)，r 保存了讀取的字節數。
    
    if (r < 0) return -1; //讀取失敗
    if (r == 0) return 0; //客戶斷開(讀到EOF)
    char* p1 = strstr(buf, "\015\012"); // \r\n  //查找 buf 中是否包含 "\015\012"，即 \r\n 的結束符。這通常是 HTTP 請求的換行符。
    if (p1 == NULL) {
        p1 = strstr(buf, "\012");   // \n //沒有找到 \r\n，繼續查找 \n 結束符
        if (p1 == NULL) { //仍然沒有找到，則檢查是否是用戶按下了 Ctrl+C
            if (!strncmp((const char *)buf, (const char *)IAC_IP, 2)) {  //strncmp 比較 buf 開頭的兩個字符和 IAC_IP（這通常是 Ctrl+C 的表示）。
                // Client presses ctrl+C, regard as disconnection
                fprintf(stderr, "Client presses ctrl+C....\n");
                return 0; //表示客戶端斷開
            }
        }
    }
    char *p2 = buf + r - 1;
    if(p1 == NULL){
        memmove(reqP->buf + reqP->buf_len, buf, p2 - buf + 1);
        reqP->buf_len += p2 - buf + 1;
        return 0;
    }
    else{
        len = p1 - buf;
        memmove(reqP->buf + reqP->buf_len, buf, len);
        reqP->buf_len += len;
        reqP->buf[reqP->buf_len] = '\0';
    }
    


    
    if(strcmp(reqP->buf, "exit") == 0){
        write(reqP->conn_fd, exit_msg, strlen(exit_msg));
        return 2;
    }
    if(reqP->status == SHIFT){
        if(!IsNumber(reqP->buf)){
            write(reqP->conn_fd, invalid_op_msg, strlen(invalid_op_msg));
            return 2;
        }
        int re = atoi(reqP->buf);
        if(!(902001 <= re && re <= 902005)){
            write(reqP->conn_fd, invalid_op_msg, strlen(invalid_op_msg));
            return 2; //不合法的車號
        }
        # ifdef READ_SERVER
        reqP->booking_info.shift_id = re;
        # endif
    }
    # ifdef WRITE_SERVER
    else if(reqP->status == SEAT){
        if(strcmp(reqP->buf, "pay") == 0){
            if(reqP->booking_info.num_of_chosen_seats == 0){
                write(reqP->conn_fd, no_seat_msg, strlen(no_seat_msg));
                print_train_info(reqP);
                write(reqP->conn_fd, write_seat_msg, strlen(write_seat_msg));
                return 1;
            }
            for(int i = 0; i < SEAT_NUM; i++){
                if(reqP->booking_info.seat_stat[i] == CHOSEN){
                    reqP->booking_info.seat_stat[i] = PAID;
                    lseek(reqP->booking_info.train_fd, i * 2,  SEEK_SET);
                    char ch = '1';
                    write(reqP->booking_info.train_fd, &ch, 1);
                }
                reqP->booking_info.num_of_chosen_seats = 0;
            }
            write(reqP->conn_fd, book_succ_msg, strlen(book_succ_msg));
            reqP->status = BOOKED;
            return 1;
        }
        if(!IsNumber(reqP->buf)){
            write(reqP->conn_fd, invalid_op_msg, strlen(invalid_op_msg));
            return 2;
        }

        int seat = atoi(reqP->buf);
        if(!(1 <= seat && seat <= 40)){
            write(reqP->conn_fd, invalid_op_msg, strlen(invalid_op_msg));
            return 2;
        }
        else{
            if(CheckBooked(reqP)){
                write(reqP->conn_fd, seat_booked_msg, strlen(seat_booked_msg));
            }
            else{
                SelectSeat(reqP);
            }
            print_train_info(reqP);
            write(reqP->conn_fd, write_seat_msg, strlen(write_seat_msg));
        }  
        return 1;
    }
    else if(reqP->status == BOOKED){
        if(strcmp(reqP->buf, "seat") == 0){
            reqP->status = SEAT;
            print_train_info(reqP);
            write(reqP->conn_fd, write_seat_msg, strlen(write_seat_msg));
        }
        else{
            write(reqP->conn_fd, invalid_op_msg, strlen(invalid_op_msg));
            return 2;
        }
        return 1;
    }
    # endif
   


    
    return 1; //成功讀取並處理客戶端的請求。
}

int main(int argc, char** argv) {
    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    int conn_fd;  // fd for file that we open for reading
    char buf[MAX_MSG_LEN*2], filename[FILE_LEN]; //buf 用來存儲消息緩衝區，filename 用來存儲每個火車文件的完整文件路徑
    
    int i,j;

    for (i = TRAIN_ID_START, j = 0; i <= TRAIN_ID_END; i++, j++) {
        getfilepath(filename, i); //getfilepath 函式生成對應 i（即火車 ID）的文件路徑，並存儲在 filename 中

#ifdef READ_SERVER
        trains[j].file_fd = open(filename, O_RDONLY); //開好火車檔案們
#elif defined WRITE_SERVER
        trains[j].file_fd = open(filename, O_RDWR);
#else
        trains[j].file_fd = -1;
#endif
        if (trains[j].file_fd < 0) { //打開文件失敗
            ERR_EXIT("open");
        }
    }

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));
    maxfd = (maxfd > 1024)? 1024: maxfd;
    struct pollfd *fds = (struct pollfd*) malloc(sizeof(pollfd) * maxfd);
    

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);
    
    //memset(fds, 0, sizeof(fds));

    for (int i = 1; i < maxfd; i++) {
        fds[i].fd = -1;
    }
    fds[STDIN_FILENO].fd = STDIN_FILENO; //監聽標準輸入設備是否有數據可讀
    fds[STDIN_FILENO].events = POLLIN; //poll 函數在標準輸入可讀（有用戶輸入）時返回。
    fds[svr.listen_fd].fd = svr.listen_fd;
    fds[svr.listen_fd].events = POLLIN;
    
    struct timeval time;
    while (1) {
        struct timeval time;
        gettimeofday(&time, NULL);
        int lefttime = 50;
        int n = poll(fds, maxfd, lefttime);

        if(fds[svr.listen_fd].revents & POLLIN){
            int new_fd = accept_conn();
            if(new_fd < 0) continue;
            for(int i = 9; i < maxfd; i++){
                if(fds[i].fd == -1){
                    fds[i].fd = new_fd;
                    fds[i].events = POLLIN;
                    break;
                }
            }
            

            write(new_fd, welcome_banner, strlen(welcome_banner));
            #ifdef READ_SERVER
            requestP[new_fd].status = SHIFT; 
            write(new_fd, read_shift_msg, strlen(read_shift_msg));
            #elif defined WRITE_SERVER
            requestP[new_fd].status = SHIFT; 
            write(new_fd, write_shift_msg, strlen(write_shift_msg));
            # endif
            fds[svr.listen_fd].revents = 0;
        }
        for(int i = 9; i < maxfd; i++){
            if(fds[i].fd == -1) continue;
            
            gettimeofday(&time, NULL);
            int millisecond = time.tv_sec * 1000 + time.tv_usec / 1000;
            if(millisecond >= requestP[fds[i].fd].remaining_time){
                unlock(&requestP[fds[i].fd]);
                close(requestP[fds[i].fd].conn_fd);
                free_request(&requestP[fds[i].fd]);
                fds[i].fd = -1;
                continue;
            }
            if(requestP[fds[i].fd].remaining_time - millisecond < lefttime) lefttime = requestP[fds[i].fd].remaining_time - millisecond;
            
            if(fds[i].fd != -1 && fds[i].revents & POLLIN){ 
                #ifdef READ_SERVER
                int ret = handle_read(&requestP[fds[i].fd]); //將客戶端發送的數據存入 requestP[conn_fd] 結構中
	            if (ret < 0) { //讀取失敗，打印錯誤信息，並繼續下一次循環
                    fprintf(stderr, "bad request from %s\n", requestP[fds[i].fd].host);
                    continue;
                }
                if(ret == 2){
                    
                    unlock(&requestP[fds[i].fd]);
                    close(requestP[fds[i].fd].conn_fd);
                    free_request(&requestP[fds[i].fd]);
                    fds[i].fd = -1;
                    continue;
                }
                if(ret == 0) continue;
                print_train_info(&requestP[fds[i].fd]);
                write(fds[i].fd, read_shift_msg, strlen(read_shift_msg));

                #elif defined WRITE_SERVER
                int ret = handle_read(&requestP[fds[i].fd]); 
	            if (ret < 0) { 
                    fprintf(stderr, "bad request from %s\n", requestP[fds[i].fd].host);
                    continue;
                }
                if(ret == 2){
                    unlock(&requestP[fds[i].fd]);
                    close(requestP[fds[i].fd].conn_fd);
                    free_request(&requestP[fds[i].fd]);
                    fds[i].fd = -1;
                    continue;
                }
                if(ret == 0) continue;
                if(requestP[fds[i].fd].status == SHIFT){
                    if(CheckAllOne(&requestP[fds[i].fd])){
                        write(requestP[fds[i].fd].conn_fd, full_msg, strlen(full_msg));
                        write(requestP[fds[i].fd].conn_fd, write_shift_msg, strlen(write_shift_msg));
                    }
                    else{
                        requestP[fds[i].fd].booking_info.shift_id = atoi(requestP[fds[i].fd].buf);
                        requestP[fds[i].fd].booking_info.train_fd = trains[requestP[fds[i].fd].booking_info.shift_id - 902001].file_fd;
                        print_train_info(&requestP[fds[i].fd]);
                        requestP[fds[i].fd].status = SEAT;
                        write(requestP[fds[i].fd].conn_fd, write_seat_msg, strlen(write_seat_msg));
                    }
                }
                else if(requestP[fds[i].fd].status == BOOKED){
                    print_train_info(&requestP[fds[i].fd]);
                    write(requestP[fds[i].fd].conn_fd, write_seat_or_exit_msg, strlen( write_seat_or_exit_msg));
                }
                #endif 
                requestP[fds[i].fd].buf_len = 0;
            }
        }

    }

    free(requestP);
    close(svr.listen_fd); //不再接受新的連接
    for (i = 0;i < TRAIN_NUM; i++)
        close(trains[i].file_fd);
    return 0;
}

int accept_conn(void) {
    struct sockaddr_in cliaddr; //用來存儲客戶端的 IP 地址和port
    size_t clilen; //存儲客戶端地址結構體的大小
    int conn_fd;  // fd(file descriptor) for a new connection with client
    struct timeval time;
    

    clilen = sizeof(cliaddr); //初始化 clilen，設置為 cliaddr 結構體的大小 
    conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen); //接受新的客戶端連接, accept(伺服器正在監聽的fd, 客戶端地址, 客戶端地址結構體的大小)。返回一個新的fd conn_fd，它代表新客戶端的連接
    if (conn_fd < 0) { //連接失敗
        if (errno == EINTR || errno == EAGAIN) return -1;  // try again
        if (errno == ENFILE) { //系統文件描述符表已滿，無法打開更多文件描述符
            (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                return -1;
        }
        ERR_EXIT("accept"); //出現其他錯誤，呼叫 ERR_EXIT 函式，輸出錯誤信息並終止程序
    }
    
    requestP[conn_fd].conn_fd = conn_fd; //新客戶端的文件描述符 conn_fd 保存到全局的 requestP 數組中，這個數組用來管理每個客戶端的請求。
    strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr)); //將客戶端的 IP 地址存到host
    fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
    requestP[conn_fd].client_id = (svr.port * 1000) + num_conn;    // This should be unique for the same machine.
    num_conn++; //每成功連接一個新客戶端後，遞增連接計數器 num_conn
    gettimeofday(&time, NULL);
    int milliseconds = time.tv_sec * 1000 + time.tv_usec / 1000;

    requestP[conn_fd].remaining_time = milliseconds + TIME_OUT;

    return conn_fd; //返回新客戶端的文件描述符，表示連接成功
}

static void getfilepath(char* filepath, int extension) {
    char fp[FILE_LEN*2]; //用來臨時存儲生成的完整文件路徑
    
    memset(filepath, 0, FILE_LEN); //將 filepath 清空
    sprintf(fp, "%s%d", file_prefix, extension); //生成一個字符串，這個字符串是由 file_prefix和 extension（整數擴展名）組成的。生成的結果存儲到 fp
    strcpy(filepath, fp); //filepath 現在會包含剛剛組合生成的完整文件路徑，會影響函式外部
}

// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->client_id = -1;
    reqP->buf_len = 0;
    reqP->status = INVALID;
    //reqP->remaining_time.tv_sec = 5;
    //reqP->remaining_time.tv_usec = 0;
    
    reqP->booking_info.shift_id = -1;
    reqP->booking_info.num_of_chosen_seats = 0;
    reqP->booking_info.train_fd = -1;
    for (int i = 0; i < SEAT_NUM; i++)
        reqP->booking_info.seat_stat[i] = UNKNOWN;
}

static void free_request(request* reqP) {
    memset(reqP, 0, sizeof(request));
    init_request(reqP);
}

static void init_server(unsigned short port) {
    memset(svr.seat_lock, -1, sizeof(svr.seat_lock));
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);
    return;
}