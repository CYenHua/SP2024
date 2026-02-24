#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <signal.h>
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <pwd.h>

#define ERR_EXIT(s) do { perror(s); exit(errno); } while(0);
#define WRITE_TO_USER 64
#define READ_FROM_USER 65
#define MAX_PATH_LEN 256
#define MAX_SUBTASK 128
#define MAX_RESULT_LEN 512
enum Result { AC, WA, TLE, RE };

// === Constraint ===
#define MAX_CHILDREN 8
#define MAX_FRIENDS 32
#define MAX_CHECK_DEPTH 7

#define MAX_FIFO_NAME_LEN 16
#define MAX_FRIEND_INFO_LEN 16
#define MAX_FRIEND_NAME_LEN 9

#define MAX_CMD_NUM 512
#define MAX_CMD_LEN 64
#define MAX_CHECK_RES_LEN 512
#define MAX_GENERAL_RES_LEN 8
#define MAX_ADOPT_BLOCK_LEN 32
// === Constraint===

// === FriendNameTable ===
const char *kFriendNames[] = {
    // Common name in 2014-2015
    "Amelia", "Olivia", "Isla", "Emily", "Poppy",
    "Oliver", "Jack", "Harry", "Jacob", "Charlie",
    "Ava", "Isabella", "Jessica", "Lily", "Sophie",
    "Thomas", "George", "Oscar", "James", "Matt",
    "Smith", "Jones", "Williams", "Brown", "Taylor",
    "Davies", "Wilson", "Evans", "Thomas", "Roberts",
    // Tea name
    "Alfalfa", "Assam", "Burdock", "Jasmine", "Ceylon",
    "Earl", "Fennel", "Silver", "Green", "Hibiscus",
    // Coffie name
    "BlackEye", "Cortado", "Doppio", "Espresso", "Irish",
    "Latte", "Lungo", "Mocha", "RedEye", "Turkish",
    // Alcohol name
    "Gin", "Vodka", "Whiskey", "Tequila", "Rum",
    "Brandy", "Amaro", "Aperol", "Campari", "Hpnotiq",
    // Flower name
    "Rose", "Lotus", "Bluebell", "Snowdrop", "Lavender",
    "Primrose", "Lilac", "Saffron", "Lantana", "Peony"
};
const int kFriendNameNum = 70;
// === FriendNameTable ===

// === FriendArray ===
// Don't copy this, since it is intentionally inefficient
typedef struct {
    char info[MAX_FRIEND_INFO_LEN];
    char name[MAX_FRIEND_NAME_LEN];
    int value;
} Friend;

Friend FriendFromInfo(char *friend_info);
Friend FriendFromNameValue(char *friend_name, int value);

#define EMPTY_SLOT -1
typedef struct {
    Friend children[MAX_CMD_NUM];
    int size, left;
} FriendArray;

FriendArray FaNew();
int FaInsert(FriendArray *fa, Friend nf);
void FaRemove(FriendArray *fa, int index);
Friend* FaAt(FriendArray *fa, int index);
int FaSize(FriendArray *fa);
bool FaEmpty(FriendArray *fa);
// === FriendArray ===

// === FriendTree ===
enum GraduateResult { ROOT, DESCENDANT, NOT_FOUND };
static inline bool is_Not_Tako(char *friend_name);
// Again, don't copy this, since you will get 0 mark for not following the spec
typedef struct node {
    Friend self_info;
    FriendArray children_info;
    struct node *child_ptr[MAX_CMD_NUM];
    struct node *parent_ptr;
} Node;

Node* NodeNew(Friend self_info);
void NodeDelete(Node *node);

typedef struct {
    Node *root;
    int size;
} Tree;

Tree TreeNew();
void TreeDelete(Tree *tree);
void TreeMeet(Tree *tree, char *parent_name, char *child_info, char *result);
Node* TreeCheck(Tree *tree, char *subtree_root_name, char *result);
int TreeGraduate(Tree *tree, char *subtree_root_name, char *result);
void TreeAdopt(Tree *tree, char *new_parent_name, char *child_name, char *result);
void TreeCompare(Tree *tree, char *friend_name, char *result);
int TreeGatherInfo(Tree *tree, char result[MAX_FRIENDS][MAX_FRIEND_INFO_LEN], int children_num[]);
int TreeGetHeight(Tree *tree, Node *cur, char all_info[MAX_FRIENDS][MAX_FRIEND_INFO_LEN], int children_height[]);
// === FriendTree ===

// === FriendQueue ===
typedef struct {
    Node *elements[MAX_FRIENDS * 2];
    int size, capacity;
    int begin, end;
} NodeQueue;

NodeQueue FqNew();
void FqDelete(NodeQueue *fq);
void FqPush(NodeQueue *fq, Node *nf);
void FqPop(NodeQueue *fq);
Node* FqFront(NodeQueue *fq);
int FqSize(NodeQueue *fq);
bool FqEmpty(NodeQueue *fq);
// === FriendQueue ===

int child_pid;
char exe_path[MAX_PATH_LEN];
char username[MAX_PATH_LEN];

static inline int max(const int a, const int b) { return a > b ? a : b; }
const char *kCharSet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
size_t char_set_len;
static inline char random_char();
static inline int random_digit();
static inline void random_name(char *buf);

enum CommandType { MEET, CHECK, GRADUATE, ADOPT, COMPARE };
const char *kMeet = "Meet";
const char *kCheck = "Check";
const char *kGraduate = "Graduate";
const char *kAdopt = "Adopt";
const char *kCompare = "Compare";
struct timeval kTimeout = { .tv_sec = 1, .tv_usec = 0 };

void print_usage();
int parse_argv(int argc, char *argv[], int subtask[MAX_SUBTASK]);
int start_friend_tree(char exe_path[MAX_PATH_LEN]);
void random_walk(int subtask, char *exe_path);

void wait_child();
// === Main function ===
// ===============================================================
//           System Programming Homework 2/4 random test          
// ===============================================================
// Usage: ./test [username] [exe_path] [-s rng_seed] [-t subtasks]
// Example 1: ./test b12902033
// Example 2: ./test b12902033 ../friend -t 1 2
// Subtask:
//     1: Meet              2: Meet & Check
//     3: Meet & Graduate   4: Meet, Check & Adopt
//     5: Mix of 1-4        6: Compare & Meet & Check
//     7: All
// Note:
//     1. The username must be correct
//     2. The order of argv matters
//     3. The tests are generated randomly with the given rng seed
//        or 0 (depend on -s)
//     4. You may select same subtask more than once
// ===============================================================
int main(int argc, char *argv[]) {
    atexit(wait_child);
    srand(0);
    print_usage();
    int subtasks[MAX_SUBTASK] = {0};
    if (parse_argv(argc, argv, subtasks) < 0) {
        exit(0);
    }
    char_set_len = strlen(kCharSet);

    for (int i = 0; i < MAX_SUBTASK; ++i) {
        if (subtasks[i] == 0) break;
        random_walk(subtasks[i], exe_path);
    }
}
// === Main function ===

void print_direct_meet(char *friend_name, char *result) {
    sprintf(result, "Not_Tako has met %s by himself\n", friend_name);
}

void print_indirect_meet(char *parent_friend_name, char *child_friend_name, char *result) {
    sprintf(result, "Not_Tako has met %s through %s\n", child_friend_name, parent_friend_name);
}

void print_fail_meet(char *parent_friend_name, char *child_friend_name, char *result) {
    sprintf(result, "Not_Tako does not know %s to meet %s\n", parent_friend_name, child_friend_name);
}

void print_fail_check(char *parent_friend_name, char *result){
    sprintf(result, "Not_Tako has checked, he doesn't know %s\n", parent_friend_name);
}

void print_success_adopt(char *parent_friend_name, char *child_friend_name, char *result) {
    sprintf(result, "%s has adopted %s\n", parent_friend_name, child_friend_name);
}

void print_fail_adopt(char *parent_friend_name, char *child_friend_name, char *result) {
    sprintf(result, "%s is a descendant of %s\n", parent_friend_name, child_friend_name);
}

void print_compare_alive(char *friend_name, char *result){
    sprintf(result, "Not_Tako is still friends with %s\n", friend_name);
}

void print_compare_dead(char *friend_name, char *result){
    sprintf(result, "%s is dead to Not_Tako\n", friend_name);
}

void print_final_graduate(char *result){
    sprintf(result, "Congratulations! You've finished Not_Tako's annoying tasks!\n");
}

Friend FriendFromInfo(char *friend_info) {
    Friend new_friend = {0};
    strncpy(new_friend.info, friend_info, MAX_FRIEND_INFO_LEN);
    if (strcmp(friend_info, "Not_Tako") == 0) {
        strncpy(new_friend.name, friend_info, MAX_FRIEND_NAME_LEN);
        new_friend.value = 100;
        return new_friend;
    }
    int index = 0;
    while (friend_info[index] != '_') index++;
    friend_info[index] = '\0';
    strncpy(new_friend.name, friend_info, MAX_FRIEND_NAME_LEN);
    new_friend.value = atoi(friend_info + index + 1);
    return new_friend;
}

Friend FriendFromNameValue(char *friend_name, int value) {
    Friend new_friend;
    snprintf(new_friend.info, MAX_FRIEND_INFO_LEN, "%s_%02d", friend_name, value);
    strncpy(new_friend.name, friend_name, MAX_FRIEND_NAME_LEN);
    new_friend.value = value;
    return new_friend;
}

FriendArray FaNew() {
    FriendArray new_friend_array;
    new_friend_array.size = 0;
    new_friend_array.left = 0;
    for (int i = 0; i < MAX_CMD_NUM; ++i) {
        new_friend_array.children[i].value = EMPTY_SLOT;
    }
    return new_friend_array;
}

int FaInsert(FriendArray *fa, Friend nf) {
    if (FaSize(fa) >= MAX_CMD_NUM) {
        ERR_EXIT("too many children in one array");
    }
    memcpy(FaAt(fa, fa->left), &nf, sizeof(Friend));
    fa->size++;
    fa->left++;
    return fa->left - 1;
    ERR_EXIT("cannot find empty space");
}

void FaRemove(FriendArray *fa, int index) {
    memset(FaAt(fa, index), 0, sizeof(Friend));
    FaAt(fa, index)->value = EMPTY_SLOT;
    fa->size--;
}

Friend* FaAt(FriendArray *fa, int index) {
    if (index < 0 || index >= MAX_CMD_NUM) {
        fprintf(stderr, "index: %d, ", index);
        ERR_EXIT("index out of range in FriendArray");
    }
    return &fa->children[index];
}

int FaSize(FriendArray *fa) {
    return fa->size;
}

bool FaEmpty(FriendArray *fa) {
    return fa->size == 0;
}

NodeQueue FqNew() {
    NodeQueue new_friend_queue = {
        .size = 0,
        .capacity = MAX_FRIENDS * 2,
        .begin = 0,
        .end = 0
    };
    return new_friend_queue;
}

void FqDelete(NodeQueue *fq) {
    fq->size = 0;
    fq->begin = fq->end = 0;
    for (int i = 0; i < fq->capacity; ++i) {
        fq->elements[i] = NULL;
    }
}

void FqPush(NodeQueue *fq, Node *new_node) {
    if (fq->size == fq->capacity) {
        // Although we can allocate twice the space, this should not happen in this program
        ERR_EXIT("queue overflow");
    }
    fq->elements[fq->end] = new_node;
    fq->end = (fq->end + 1) % fq->capacity;
    fq->size++;
}

void FqPop(NodeQueue *fq) {
    if (fq->begin == fq->end) {
        ERR_EXIT("pop from an empty queue");
    }
    fq->elements[fq->begin] = NULL;
    fq->begin = (fq->begin + 1) % fq->capacity;
    fq->size--;
}

Node* FqFront(NodeQueue *fq) {
    return fq->elements[fq->begin];
}

int FqSize(NodeQueue *fq) {
    return fq->size;
}

bool FqEmpty(NodeQueue *fq) {
    return fq->size == 0;
}

static inline bool is_Not_Tako(char *friend_name) {
    return strcmp(friend_name, "Not_Tako") == 0;
}

Node* NodeNew(Friend self_info) {
    Node *new_node = calloc(1, sizeof(Node));
    new_node->self_info = self_info;
    new_node->children_info = FaNew();
    new_node->parent_ptr = NULL;
    for (int i = 0; i < MAX_CHILDREN; ++i) {
        new_node->child_ptr[i] = NULL;
    }
    return new_node;
}

void NodeDelete(Node *node) {
    free(node);
}

Tree TreeNew() {
    Tree new_tree = {
        .root = calloc(1, sizeof(Node)),
        .size = 1
    };
    new_tree.root = NodeNew(FriendFromInfo("Not_Tako"));
    return new_tree;
}

void TreeDelete(Tree *tree) {
    NodeQueue queue = FqNew();
    FqPush(&queue, tree->root);
    while (!FqEmpty(&queue)) {
        Node *cur = FqFront(&queue);
        FqPop(&queue);
        int finished = 0;
        for (int i = 0; i < MAX_CMD_NUM; ++i) {
            if (FaAt(&cur->children_info, i)->value != -1) {
                FqPush(&queue, cur->child_ptr[i]);
                finished++;
            }
            if (finished >= FaSize(&cur->children_info)) break;
        }
        NodeDelete(cur);
    }
    FqDelete(&queue);
}

void TreeMeet(Tree *tree, char *parent_name, char *child_info, char *result) {
    Friend new_friend = FriendFromInfo(child_info);
    // Direct meet
    if (is_Not_Tako(parent_name)) { 
        int index = FaInsert(&tree->root->children_info, new_friend);
        tree->root->child_ptr[index] = NodeNew(new_friend);
        tree->root->child_ptr[index]->parent_ptr = tree->root;
        tree->size++;
        print_direct_meet(new_friend.name, result);
        return;
    }
    // Indirect meet
    NodeQueue queue = FqNew();
    FqPush(&queue, tree->root);
    bool find_parent = false;
    while (!FqEmpty(&queue)) {
        Node *cur = FqFront(&queue);
        FqPop(&queue);
        if (strcmp(cur->self_info.name, parent_name) == 0) {
            int index = FaInsert(&cur->children_info, new_friend);
            cur->child_ptr[index] = NodeNew(new_friend);
            cur->child_ptr[index]->parent_ptr = cur;
            find_parent = true;
            tree->size++;
            break;
        }
        int finished = 0;
        for (int i = 0; i < MAX_CMD_NUM; ++i) {
            if (FaAt(&cur->children_info, i)->value != EMPTY_SLOT) {
                FqPush(&queue, cur->child_ptr[i]);
                finished++;
            }
            if (finished >= FaSize(&cur->children_info)) break;
        }
    }
    if (find_parent) {
        print_indirect_meet(parent_name, new_friend.name, result);
    }
    else {
        print_fail_meet(parent_name, new_friend.name, result);
    }
    FqDelete(&queue);
}

Node* TreeCheck(Tree *tree, char *subtree_root_name, char *result) {
    NodeQueue queue = FqNew();
    FqPush(&queue, tree->root);
    Node *subtree_root = NULL;
    while (!FqEmpty(&queue)) {
        Node *cur = FqFront(&queue);
        FqPop(&queue);
        if (strcmp(cur->self_info.name, subtree_root_name) == 0) {
            subtree_root = cur;
            break;
        }
        int finished = 0;
        for (int i = 0; i < MAX_CMD_NUM; ++i) {
            if (FaAt(&cur->children_info, i)->value != EMPTY_SLOT) {
                FqPush(&queue, cur->child_ptr[i]);
                finished++;
            }
            if (finished >= FaSize(&cur->children_info)) break;
        }
    }
    FqDelete(&queue);
    // Root not found
    if (subtree_root == NULL) {
        print_fail_check(subtree_root_name, result);
        return NULL;
    }
    NodeQueue now = FqNew(), next = FqNew();
    FqPush(&now, subtree_root);
    int layer = 0, result_len = 0;
    while (1) {
        int layer_node_num = 0;
        if (layer > MAX_FRIENDS) ERR_EXIT("check too deep");
        while (!FqEmpty(&now)) {
            Node *cur = FqFront(&now);
            FqPop(&now);
            strcpy(result + result_len, cur->self_info.info);
            result_len += strlen(cur->self_info.info);
            if (!FqEmpty(&now)) {
                result[result_len++] = ' ';
                result[result_len] = '\0';
            }
            int finished = 0;
            for (int i = 0; i < MAX_CMD_NUM; ++i) {
                if (FaAt(&cur->children_info, i)->value != EMPTY_SLOT) {
                    FqPush(&next, cur->child_ptr[i]);
                    finished++;
                }
                if (finished >= FaSize(&cur->children_info)) break;
            }
            layer_node_num++;
        }
        FqDelete(&now);
        if (layer_node_num == 0) break;
        result[result_len++] = '\n';
        result[result_len] = '\0';
        now = next;
        next = FqNew();
        layer++;
    }
    return subtree_root;
}

int TreeGraduate(Tree *tree, char *subtree_root_name, char *result) {
    Node *subtree_root = TreeCheck(tree, subtree_root_name, result);
    if (subtree_root == NULL) {
        return NOT_FOUND;
    }
    int result_len = strlen(result);
    bool find_itself = false;

    Node *parent = subtree_root->parent_ptr;
    if (parent != NULL) {
        for (int i = 0; i < MAX_CMD_NUM; ++i) {
            if (FaAt(&parent->children_info, i)->value != EMPTY_SLOT && 
                strcmp(FaAt(&parent->children_info, i)->name, subtree_root_name) == 0
            ) {
                FaRemove(&parent->children_info, i);
                parent->child_ptr[i] = NULL;
                find_itself = true;
                break;
            }
        }
    }
    NodeQueue queue = FqNew();
    FqPush(&queue, subtree_root);
    while (!FqEmpty(&queue)) {
        Node *cur = FqFront(&queue);
        FqPop(&queue);
        int finished = 0;
        for (int i = 0; i < MAX_CMD_NUM; ++i) {
            if (FaAt(&cur->children_info, i)->value != -1) {
                FqPush(&queue, cur->child_ptr[i]);
                finished++;
            }
            if (finished >= FaSize(&cur->children_info)) break;
        }
        NodeDelete(cur);
        tree->size--;
    }
    FqDelete(&queue);
    if (is_Not_Tako(subtree_root_name)) {
        print_final_graduate(result + result_len);
        return ROOT;
    }
    return DESCENDANT;
}

void TreeAdopt(Tree *tree, char *new_parent_name, char *child_name, char *result) {
    char dummy_result[MAX_RESULT_LEN] = {0};
    Node *child = TreeCheck(tree, child_name, dummy_result);
    if (strstr(dummy_result, new_parent_name) != NULL) {
        print_fail_adopt(new_parent_name, child_name, result);
        return;
    }
    memset(dummy_result, 0, sizeof(dummy_result));
    Node *new_parent = TreeCheck(tree, new_parent_name, dummy_result);
    // Adjust friend value
    NodeQueue queue = FqNew();
    FqPush(&queue, child);
    while (!FqEmpty(&queue)) {
        Node *cur = FqFront(&queue);
        FqPop(&queue);
        
        snprintf(cur->self_info.info, MAX_FRIEND_INFO_LEN, "%s_%02d", 
            cur->self_info.name, cur->self_info.value % new_parent->self_info.value);
        if (new_parent->self_info.value == 0) ERR_EXIT("devide by zero in TreeAdopt function");
        cur->self_info.value %= new_parent->self_info.value;

        int finished = 0;
        for (int i = 0; i < MAX_CMD_NUM; ++i) {
            if (FaAt(&cur->children_info, i)->value != -1) {
                FqPush(&queue, cur->child_ptr[i]);
                finished++;
            }
            if (finished >= FaSize(&cur->children_info)) break;
        }
    }
    FqDelete(&queue);
    bool find_itself = false;
    Node *parent = child->parent_ptr;
    if (parent == NULL) ERR_EXIT("cannot find old parent of the child");
    for (int i = 0; i < MAX_CMD_NUM; ++i) {
        if (FaAt(&parent->children_info, i)->value != EMPTY_SLOT && 
            strcmp(FaAt(&parent->children_info, i)->name, child->self_info.name) == 0
        ) {
            FaRemove(&parent->children_info, i);
            parent->child_ptr[i] = NULL;
            find_itself = true;
            break;
        }
    }
    if (!find_itself) ERR_EXIT("fail to find itself");
    int new_index = FaInsert(&new_parent->children_info, child->self_info);
    new_parent->child_ptr[new_index] = child;
    child->parent_ptr = new_parent;
    
    print_success_adopt(new_parent_name, child_name, result);
}

// TODO
void TreeCompare(Tree *tree, char *friend_name, char *result) {

}
// TODO

int TreeGatherInfo(Tree *tree, char result[MAX_FRIENDS][MAX_FRIEND_INFO_LEN], int children_num[]) {
    NodeQueue queue = FqNew();
    FqPush(&queue, tree->root);
    int result_index = 0;
    while (!FqEmpty(&queue)) {
        Node *cur = FqFront(&queue);
        FqPop(&queue);
        strncpy(result[result_index], cur->self_info.info, MAX_FRIEND_INFO_LEN);
        int finished = 0;
        for (int i = 0; i < MAX_CMD_NUM; ++i) {
            if (FaAt(&cur->children_info, i)->value != EMPTY_SLOT) {
                FqPush(&queue, cur->child_ptr[i]);
                children_num[result_index]++;
                finished++;
            }
            if (finished >= FaSize(&cur->children_info)) break;
        }
        result_index++;
    }
    FqDelete(&queue);
    if (result_index != tree->size) {
        fprintf(stderr, "find ret index: %d, while tree size: %d, ", result_index, tree->size);
        ERR_EXIT("the tree size should equal to result index");
    }
    return result_index;
}

int TreeGetHeight(Tree *tree, Node *cur, char all_info[MAX_FRIENDS][MAX_FRIEND_INFO_LEN], int children_height[]) {
    int cur_index = -1;
    for (int i = 0; i < tree->size; ++i) {
        if (strcmp(all_info[i], cur->self_info.info) == 0) {
            cur_index = i;
            break;
        }
    }
    if (cur_index < 0) ERR_EXIT("cannot find cur in the tree while geting height");
    children_height[cur_index] = 0;
    int finished = 0;
    for (int i = 0; i < MAX_CMD_NUM; ++i) {
        if (FaAt(&cur->children_info, i)->value != EMPTY_SLOT) {
            children_height[cur_index] = 
                max(children_height[cur_index], 
                    TreeGetHeight(tree, cur->child_ptr[i], all_info, children_height) + 1);
            finished++;
        }
        if (finished >= FaSize(&cur->children_info)) break;
    }
    return children_height[cur_index];
}

static inline char random_char() {
    size_t index = (double) rand() / RAND_MAX * char_set_len;
    return kCharSet[index];
}

static inline int random_digit() {
    int digit = (double) rand() / RAND_MAX * 9;
    return digit;
}

static inline int random_num(int low, int high) {
    const int kRange = high - low + 1;
    int num = (double) rand() / RAND_MAX * kRange;
    return (num + low > high) ? high : num + low;
}

static inline void random_name(char *buf) {
    const int kIndex = random_num(0, kFriendNameNum - 1);
    strncpy(buf, kFriendNames[kIndex], MAX_FRIEND_NAME_LEN);
}

void print_usage() {
    fprintf(stderr, 
        "\e[1;36m===============================================================\e[0m\n"
        "\e[1;36m          System Programming Homework 2/4 random test          \e[0m\n"
        "\e[1;36m===============================================================\e[0m\n"
        "Usage: ./test [username] [exe_path] [-s rng_seed] [-t subtasks]\n"
        "Example 1: ./test b12902033\n"
        "Example 2: ./test b12902033 ../friend -t 1 2\n"
        "Subtask:\n"
        "    1: Meet              2: Meet & Check\n"
        "    3: Meet & Graduate   4: Meet, Check & Adopt\n"
        "    5: Mix of 1-4        6: Compare & Meet & Check\n"
        "    7: All\n"
        "\e[1;31mNote:\e[0m\n"
        "\e[1;31m    1. The username must be correct\e[0m\n"
        "\e[1;31m    2. The order of argv matters\e[0m\n"
        "\e[1;31m    3. The tests are generated randomly with the given rng seed\e[0m\n"
        "\e[1;31m       or 0 (depend on -s)\e[0m\n"
        "\e[1;31m    4. You may select same subtask more than once\e[0m\n"
        "\e[1;36m===============================================================\e[0m\n\n"
    );
}

int parse_argv(int argc, char *argv[], int subtasks[MAX_SUBTASK]) {
    if (argc == 1) {
        fprintf(stderr, "\e[1;31mError: the username is required for cleaning processes\e[0m\n");
        return -1;
    }
    // Use ./friend as exe path and test all
    strncpy(username, argv[1], MAX_PATH_LEN);
    struct passwd *pws;
    pws = getpwuid(getuid());
    if (strcmp(username, pws->pw_name) != 0) {
        fprintf(stderr, "\e[1;31mError: the username is not you\e[0m\n");
        return -1;
    }
    
    if (argc == 2) {
        if (strcmp(argv[1], "-h") == 0) return -1;
        strcpy(exe_path, "./friend");
        for (int i = 0; i < MAX_SUBTASK; ++i) {
            subtasks[i] = i + 1;
        }
        return 0;
    }
    // Use given exe path and test all
    if (argc == 3) {
        if (strlen(argv[2]) >= MAX_PATH_LEN) {
            fprintf(stderr, "\e[1;31mError: the path length should be less than %d\e[0m\n", MAX_PATH_LEN);
            return -1;
        }
        strncpy(exe_path, argv[2], MAX_PATH_LEN);
        for (int i = 0; i < MAX_SUBTASK; ++i) {
            subtasks[i] = i + 1;
        }
        return 0;
    }
    int shift = 0;
    if (argc >= 5 && strcmp(argv[3], "-s") == 0) {
        for (int j = 0, len = strlen(argv[4]); j < len; ++j)
            if (!isdigit(argv[4][j])) return -1;
        srand(atoi(argv[2]));
        shift = 2;
        if (argc == 5) {
            for (int i = 0; i < MAX_SUBTASK; ++i) {
                subtasks[i] = i + 1;
            }
            return 0;
        }
    }
    // Use given exe path and subtasks
    if (argc - 4 - shift < MAX_SUBTASK && strcmp(argv[3 + shift], "-t") == 0) {
        for (int i = 4 + shift; i < argc; ++i) {
            for (int j = 0, len = strlen(argv[i]); j < len; ++j)
                if (!isdigit(argv[i][j])) return -1;
            int subtask = atoi(argv[i]);
            if (subtask < 0 || subtask > 7) {
                fprintf(stderr, "\e[1;31mError: the subtask number should be in [%d, %d]\e[0m\n", 1, 7);
                return -1;
            }
            subtasks[i - 4 - shift] = subtask;
        }
        return 0;
    }
    else if (strcmp(argv[3 + shift], "-t") != 0) {
        fprintf(stderr, "\e[1;31mError: expect -t flag after exe_path or seed\e[0m\n");
        return -1;
    }
    fprintf(stderr, "\e[1;31mError: too many subtasks, limit %d\e[0m\n", MAX_SUBTASK);
    return -1;
}

int start_friend_tree(char exe_path[MAX_PATH_LEN]) {
    int user_read_pipe[2] = {0}, user_write_pipe[2] = {0};
    pipe(user_read_pipe);
    pipe(user_write_pipe);

    int pid = fork();
    if (pid == 0) {
        close(user_read_pipe[1]);
        close(user_write_pipe[0]);
        if (user_read_pipe[0] != STDIN_FILENO) {
            if (dup2(user_read_pipe[0], STDIN_FILENO) < 0)
                ERR_EXIT("fail to dup the fd to STDIN_FILENO");
        }
        if (user_write_pipe[1] != STDOUT_FILENO) {
            if (dup2(user_write_pipe[1], STDOUT_FILENO) < 0)
                ERR_EXIT("fail to dup the fd to STDOUT_FILENO");
        }
        // Also redirect user's stderr to the output
        // comment it to disable
        if (user_write_pipe[1] != STDERR_FILENO) {
            if (dup2(user_write_pipe[1], STDERR_FILENO) < 0)
                perror("fail to dup the fd to STDOUT_FILENO");
        }
        close(user_read_pipe[0]);
        close(user_write_pipe[1]);
        int result = execl("./friend", "./friend", "Not_Tako", (char*) NULL);
        if (result < 0) {
            ERR_EXIT("fail to execute the program");
        }
        ERR_EXIT("the program should return to here");
    }
    else if (pid < 0) {
        ERR_EXIT("fail to fork a process");
    }
    close(user_read_pipe[0]);
    close(user_write_pipe[1]);
    if (user_read_pipe[1] != WRITE_TO_USER) {
        if (dup2(user_read_pipe[1], WRITE_TO_USER) < 0) 
            ERR_EXIT("fail to dup the fd to WRITE_TO_USER");
    }
    if (user_write_pipe[0] != READ_FROM_USER) {
        if (dup2(user_write_pipe[0], READ_FROM_USER) < 0)
            ERR_EXIT("fail to dup the fd to READ_FROM_USER");
    }
    close(user_read_pipe[1]);
    close(user_write_pipe[0]);
    child_pid = pid;
    return child_pid;
}

void print_progress_bar(int percent) {
    const int kPreservedCol = 19;
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    int progress_col = (double) (w.ws_col - kPreservedCol) / 100 * percent;

    fprintf(stderr, "\r         [%3d%%] [", percent);
    for (int i = 0; i < progress_col; ++i) {
        fprintf(stderr, "#");
    }
    for (int i = 0; i < w.ws_col - progress_col - kPreservedCol; ++i) {
        fprintf(stderr, ".");
    }
    fprintf(stderr, "]");
}

int write_command_wait_user(char *ref_result, char *cmd) {
    int ref_result_len = strlen(ref_result);
    write(WRITE_TO_USER, cmd, strlen(cmd));
    
    // Wait the user response for kTimeout
    struct timeval current_time, start_time, singleReadTime = { .tv_sec = 0, .tv_usec = 200 * 1000};
    gettimeofday(&current_time, NULL);
    start_time = current_time;
    char usr_result[MAX_RESULT_LEN] = {0};
    int usr_result_len = 0;
    while (1) {
        struct timeval timeout = kTimeout;
        fd_set read_set = {0};
        FD_SET(READ_FROM_USER, &read_set);
        int select_result = select(READ_FROM_USER + 1, &read_set, NULL, NULL, &timeout);

        if (select_result == 0) {
            break;
        }
        else if (select_result < 0) {
            return RE;
        }
        gettimeofday(&current_time, NULL);
        if (
            current_time.tv_sec - start_time.tv_sec > kTimeout.tv_sec || 
            (
                current_time.tv_sec - start_time.tv_sec == kTimeout.tv_sec && 
                current_time.tv_usec - start_time.tv_usec > kTimeout.tv_usec
            )
        ) {
            return TLE;
        }
        int read_len = read(READ_FROM_USER, usr_result + usr_result_len, MAX_RESULT_LEN - usr_result_len);
        if (read_len < 0) ERR_EXIT("fail to read user response");
        usr_result_len += read_len;
        if (usr_result_len >= ref_result_len) break;
    }

    if (strcmp(ref_result, usr_result) == 0) {
        return AC;
    }
    else if (strlen(usr_result) == 0) {
        return TLE;
    }
    else {
        fprintf(stderr, " Ref: [%s]\n", ref_result);
        fprintf(stderr, "Usr: [%s]\n", usr_result);
        return WA;
    }
}

int meet_run_command(Tree *ref, char *parent_name, char *child_info, char *cmd) {
    char ref_result[MAX_RESULT_LEN] = {0};
    TreeMeet(ref, parent_name, child_info, ref_result);
    return write_command_wait_user(ref_result, cmd);
}

int meet_valid_command(Tree *ref) {
    char all_info[MAX_FRIENDS][MAX_FRIEND_INFO_LEN] = {0};
    int child_num[MAX_FRIENDS] = {0};
    int tree_size = TreeGatherInfo(ref, all_info, child_num);
    int parent_index;
    do {
        parent_index = random_num(0, tree_size - 1);
    } while (child_num[parent_index] >= MAX_CHILDREN);
    for (int j = 1; j < tree_size; ++j) {
        strtok(all_info[j], "_");
    }

    char parent_name[MAX_FRIEND_NAME_LEN] = {0};
    strncpy(parent_name, all_info[parent_index], MAX_FRIEND_NAME_LEN);
    char child_info[MAX_FRIEND_INFO_LEN] = {0};
    int child_index = 0;
    bool find = false;
    do {
        find = false;
        child_index = random_num(0, kFriendNameNum - 1);
        for (int j = 0; j < tree_size; ++j) {
            if (strcmp(all_info[j], kFriendNames[child_index]) == 0) {
                find = true;
                break;
            }
        }
    } while (find);
    snprintf(child_info, MAX_FRIEND_INFO_LEN, "%s_%02d", kFriendNames[child_index], random_num(0, 99));

    char cmd[MAX_CMD_LEN] = {0};
    snprintf(cmd, MAX_CMD_LEN, "%s %s %s\n", kMeet, parent_name, child_info);

    // fprintf(stderr, "%s", cmd);
    
    return meet_run_command(ref, parent_name, child_info, cmd);
}

int meet_invalid_command(Tree *ref) {
    char all_info[MAX_FRIENDS][MAX_FRIEND_INFO_LEN] = {0};
    int child_num[MAX_FRIENDS] = {0};
    int tree_size = TreeGatherInfo(ref, all_info, child_num);
    int parent_name_len = random_num(1, MAX_FRIEND_NAME_LEN - 1);
    char parent_name[MAX_FRIEND_NAME_LEN] = {0};
    for (int i = 0; i < parent_name_len; ++i) {
        parent_name[i] = random_char();
    }
    char child_info[MAX_FRIEND_INFO_LEN] = {0};
    int child_index = 0;
    bool find = false;
    do {
        find = false;
        child_index = random_num(0, kFriendNameNum - 1);
        for (int j = 0; j < tree_size; ++j) {
            if (strcmp(all_info[j], kFriendNames[child_index]) == 0) {
                find = true;
                break;
            }
        }
    } while (find);
    snprintf(child_info, MAX_FRIEND_INFO_LEN, "%s_%02d", kFriendNames[child_index], random_num(0, 99));

    char cmd[MAX_CMD_LEN] = {0};
    snprintf(cmd, MAX_CMD_LEN, "%s %s %s\n", kMeet, parent_name, child_info);

    // fprintf(stderr, "%s", cmd);
    
    return meet_run_command(ref, parent_name, child_info, cmd);
}

int check_run_command(Tree *ref, char *subtree_root, char *cmd) {
    char ref_result[MAX_RESULT_LEN] = {0};
    TreeCheck(ref, subtree_root, ref_result);
    int ref_result_len = strlen(ref_result);
    return write_command_wait_user(ref_result, cmd);
}

int check_valid_command(Tree *ref) {
    char all_info[MAX_FRIENDS][MAX_FRIEND_INFO_LEN] = {0};
    int child_num[MAX_FRIENDS] = {0};
    int tree_size = TreeGatherInfo(ref, all_info, child_num);
    int height[MAX_FRIENDS] = {0};
    TreeGetHeight(ref, ref->root, all_info, height);
    int parent_index;
    do {
        parent_index = random_num(0, tree_size - 1);
    } while (height[parent_index] >= MAX_CHECK_DEPTH - 1);
    for (int j = 1; j < tree_size; ++j) {
        strtok(all_info[j], "_");
    }

    char subtree_root_name[MAX_FRIEND_NAME_LEN] = {0};
    strncpy(subtree_root_name, all_info[parent_index], MAX_FRIEND_NAME_LEN);

    char cmd[MAX_CMD_LEN] = {0};
    snprintf(cmd, MAX_CMD_LEN, "%s %s\n", kCheck, subtree_root_name);

    // fprintf(stderr, "%s", cmd);

    return check_run_command(ref, subtree_root_name, cmd);
}

int check_invalid_command(Tree *ref) {
    char all_info[MAX_FRIENDS][MAX_FRIEND_INFO_LEN] = {0};
    int child_num[MAX_FRIENDS] = {0};
    int tree_size = TreeGatherInfo(ref, all_info, child_num);
    int parent_name_len = random_num(1, MAX_FRIEND_NAME_LEN - 1);
    char subtree_root_name[MAX_FRIEND_NAME_LEN] = {0};
    for (int i = 0; i < parent_name_len; ++i) {
        subtree_root_name[i] = random_char();
    }
    char cmd[MAX_CMD_LEN] = {0};
    snprintf(cmd, MAX_CMD_LEN, "%s %s\n", kCheck, subtree_root_name);

    // fprintf(stderr, "%s", cmd);

    return check_run_command(ref, subtree_root_name, cmd);
}

int graduate_run_command(Tree *ref, char *subtree_root, char *cmd) {
    char ref_result[MAX_RESULT_LEN] = {0};
    if (TreeGraduate(ref, subtree_root, ref_result) == ROOT)
        *ref = TreeNew();
    return write_command_wait_user(ref_result, cmd);
}

int graduate_valid_command(Tree *ref, double root_factor) {
    char all_info[MAX_FRIENDS][MAX_FRIEND_INFO_LEN] = {0};
    int child_num[MAX_FRIENDS] = {0};
    int tree_size = TreeGatherInfo(ref, all_info, child_num);
    int height[MAX_FRIENDS] = {0};
    TreeGetHeight(ref, ref->root, all_info, height);
    int parent_index;
    double can_be_root;
    do {
        parent_index = random_num(0, tree_size - 1);
        can_be_root = (double) rand() / RAND_MAX;
    } while (
        height[parent_index] >= MAX_CHECK_DEPTH - 1 || 
        (parent_index == 0 && can_be_root >= root_factor)
    );
    for (int j = 1; j < tree_size; ++j) {
        strtok(all_info[j], "_");
    }

    char subtree_root_name[MAX_FRIEND_NAME_LEN] = {0};
    strncpy(subtree_root_name, all_info[parent_index], MAX_FRIEND_NAME_LEN);

    char cmd[MAX_CMD_LEN] = {0};
    snprintf(cmd, MAX_CMD_LEN, "%s %s\n", kGraduate, subtree_root_name);

    // fprintf(stderr, "%s", cmd);

    return graduate_run_command(ref, subtree_root_name, cmd);
}

int graduate_invalid_command(Tree *ref) {
    char all_info[MAX_FRIENDS][MAX_FRIEND_INFO_LEN] = {0};
    int child_num[MAX_FRIENDS] = {0};
    int tree_size = TreeGatherInfo(ref, all_info, child_num);
    int parent_name_len = random_num(1, MAX_FRIEND_NAME_LEN - 1);
    char subtree_root_name[MAX_FRIEND_NAME_LEN] = {0};
    for (int i = 0; i < parent_name_len; ++i) {
        subtree_root_name[i] = random_char();
    }
    char cmd[MAX_CMD_LEN] = {0};
    snprintf(cmd, MAX_CMD_LEN, "%s %s\n", kCheck, subtree_root_name);

    // fprintf(stderr, "%s", cmd);

    return graduate_run_command(ref, subtree_root_name, cmd);
}

int adopt_run_command(Tree *ref, char *new_parent_name, char *child_name, char *cmd) {
    char ref_result[MAX_RESULT_LEN] = {0};
    TreeAdopt(ref, new_parent_name, child_name, ref_result);
    return write_command_wait_user(ref_result, cmd);
}

int adopt_valid_command(Tree *ref) {
    char all_info[MAX_FRIENDS][MAX_FRIEND_INFO_LEN] = {0};
    int child_num[MAX_FRIENDS] = {0};
    int tree_size = TreeGatherInfo(ref, all_info, child_num);
    int height[MAX_FRIENDS] = {0};
    TreeGetHeight(ref, ref->root, all_info, height);
    for (int j = 1; j < tree_size; ++j) {
        strtok(all_info[j], "_");
    }
    Node *new_parent = NULL;
    char *str = NULL;
    int parent_index, child_index;
    char dummy_result[MAX_RESULT_LEN] = {0};
    int iter = 0;
    do {
        iter++;
        if (iter > MAX_FRIENDS * MAX_FRIENDS) return AC;
        parent_index = random_num(0, tree_size - 1);
        child_index = random_num(0, tree_size - 1);
        if (height[child_index] < MAX_CHECK_DEPTH) {
            TreeCheck(ref, all_info[child_index], dummy_result);
            str = strstr(dummy_result, all_info[parent_index]);
        }
        else {
            continue;
        }
        new_parent = TreeCheck(ref, all_info[parent_index], dummy_result);
        if (new_parent->self_info.value == 0) {
            continue;
        }
    } while (
        height[child_index] >= MAX_CHECK_DEPTH - 1 ||
        child_index == parent_index || child_num[parent_index] >= MAX_CHILDREN || 
        str != NULL || new_parent->self_info.value == 0
    );
    if (str != NULL) {
        ERR_EXIT("fail to find pair");
    }
    if (new_parent->self_info.value == 0) ERR_EXIT("still choose a node as a new parent with value 0");

    char new_parent_name[MAX_FRIEND_NAME_LEN] = {0};
    char child_name[MAX_FRIEND_NAME_LEN] = {0};
    strncpy(new_parent_name, all_info[parent_index], MAX_FRIEND_NAME_LEN);
    strncpy(child_name, all_info[child_index], MAX_FRIEND_NAME_LEN);

    char cmd[MAX_CMD_LEN] = {0};
    snprintf(cmd, MAX_CMD_LEN, "%s %s %s\n", kAdopt, new_parent_name, child_name);

    // fprintf(stderr, "%s", cmd);

    return adopt_run_command(ref, new_parent_name, child_name, cmd);
}

int adopt_invalid_command(Tree *ref) {
    char all_info[MAX_FRIENDS][MAX_FRIEND_INFO_LEN] = {0};
    int child_num[MAX_FRIENDS] = {0};
    int tree_size = TreeGatherInfo(ref, all_info, child_num);
    int height[MAX_FRIENDS] = {0};
    TreeGetHeight(ref, ref->root, all_info, height);
    for (int j = 1; j < tree_size; ++j) {
        strtok(all_info[j], "_");
    }
    char *str = NULL;
    int parent_index, child_index;
    do {
        parent_index = random_num(0, tree_size - 1);
        child_index = random_num(0, tree_size - 1);
        char dummy_result[MAX_RESULT_LEN] = {0};
        if (height[child_index] < MAX_CHECK_DEPTH) {
            TreeCheck(ref, all_info[child_index], dummy_result);
            str = strstr(dummy_result, all_info[parent_index]);
            // fprintf(stderr, "find %s in [%s]\n", all_info[parent_index], dummy_result);
        }
        else {
            continue;
        }
    } while (
        height[child_index] >= MAX_CHECK_DEPTH - 1 || height[parent_index] >= MAX_CHECK_DEPTH - 1 ||
        child_index == parent_index || child_num[parent_index] >= MAX_CHILDREN || 
        str == NULL
    );

    char new_parent_name[MAX_FRIEND_NAME_LEN] = {0};
    char child_name[MAX_FRIEND_NAME_LEN] = {0};
    strncpy(new_parent_name, all_info[parent_index], MAX_FRIEND_NAME_LEN);
    strncpy(child_name, all_info[child_index], MAX_FRIEND_NAME_LEN);

    char cmd[MAX_CMD_LEN] = {0};
    snprintf(cmd, MAX_CMD_LEN, "%s %s %s\n", kAdopt, new_parent_name, child_name);

    // fprintf(stderr, "%s", cmd);

    return adopt_run_command(ref, new_parent_name, child_name, cmd);
}

int subtask1(char *exe_path) {
    { // Valid Commands
        fprintf(stderr, "    1-1: Valid commands\n");
        const int kTestTimes = 1000;
        const int kProgressBlockSize = kTestTimes / 100;

        int result = AC;
        for (int i = 1; i <= kTestTimes; ++i) {
            start_friend_tree(exe_path);
            Tree ref = TreeNew();
            int iterations = random_num(1, MAX_FRIENDS - 2);
            for (int j = 0; j < iterations; ++j) {
                int iter_result = meet_valid_command(&ref);
                if (iter_result != AC) {
                    result = iter_result;
                    break;
                }
            }
            TreeDelete(&ref);
            wait_child();
            if (i % kProgressBlockSize == 0) {
                print_progress_bar(i / kProgressBlockSize);
            }
            if (result != AC) break;
        }
        fprintf(stderr, "\n");
        if (result != AC) return result;
    }
    { // Mixed
        int result = AC;
        fprintf(stderr, "    1-2: Mixed valid/invalid commands\n");
        const int kTestTimes = 1000;
        const int kProgressBlockSize = kTestTimes / 100;

        for (int i = 1; i <= kTestTimes; ++i) {
            start_friend_tree(exe_path);
            Tree ref = TreeNew();
            int max_tree_size = random_num(1, 31);
            int max_iterations = random_num(max_tree_size, 128);
            int iterations = 0;
            while (ref.size < max_tree_size && iterations < max_iterations) {
                int iter_result = meet_valid_command(&ref);
                if (iter_result != AC) {
                    result = iter_result;
                    break;
                }
                iterations++;
            }
            TreeDelete(&ref);
            wait_child();
            if (i % kProgressBlockSize == 0) {
                print_progress_bar(i / kProgressBlockSize);
            }
            if (result != AC) break;
        }
        fprintf(stderr, "\n");
        if (result != AC) return result;
    }
    return AC;
}

int subtask2(char *exe_path) {
    { // Valid Commands
        fprintf(stderr, "    2-1: Valid commands\n");
        const int kTestTimes = 1000;
        const int kProgressBlockSize = kTestTimes / 100;

        const int kAllowedCmd[] = { MEET, CHECK };
        int result = AC;
        for (int i = 1; i <= kTestTimes; ++i) {
            start_friend_tree(exe_path);
            Tree ref = TreeNew();
            int iterations = random_num(32, 128);
            for (int j = 0; j < iterations; ++j) {
                int command_type;
                if (ref.size < MAX_FRIENDS - 1) {
                    command_type = kAllowedCmd[random_num(0, 1)];
                }
                else {
                    command_type = CHECK;
                }
                int iter_result;
                if (command_type == MEET) {
                    iter_result = meet_valid_command(&ref);
                }
                else if (command_type == CHECK) {
                    iter_result = check_valid_command(&ref);
                }
                else ERR_EXIT("invalid command type in subtask 2");
                if (iter_result != AC) {
                    result = iter_result;
                    break;
                }
            }
            TreeDelete(&ref);
            wait_child();
            if (i % kProgressBlockSize == 0) {
                print_progress_bar(i / kProgressBlockSize);
            }
            if (result != AC) break;
        }
        fprintf(stderr, "\n");
        if (result != AC) return result;
    }
    { // Mixed
        fprintf(stderr, "    2-2: Mixed valid/invalid commands\n");
        const int kTestTimes = 1000;
        const int kProgressBlockSize = kTestTimes / 100;

        const int kAllowedCmd[] = { MEET, CHECK };
        int result = AC;
        for (int i = 1; i <= kTestTimes; ++i) {
            start_friend_tree(exe_path);
            Tree ref = TreeNew();
            int iterations = random_num(64, 512);
            for (int j = 0; j < iterations; ++j) {
                int command_type, valid = random_num(0, 1);
                if (ref.size < MAX_FRIENDS - 1 || !valid) {
                    command_type = kAllowedCmd[random_num(0, 1)];
                }
                else {
                    command_type = CHECK;
                }
                int iter_result;
                if (command_type == MEET) {
                    if (valid) iter_result = meet_valid_command(&ref);
                    else iter_result = meet_invalid_command(&ref);
                }
                else if (command_type == CHECK) {
                    if (valid) iter_result = check_valid_command(&ref);
                    else iter_result = check_invalid_command(&ref);
                }
                else ERR_EXIT("invalid command type in subtask 2");
                if (iter_result != AC) {
                    result = iter_result;
                    break;
                }
            }
            TreeDelete(&ref);
            wait_child();
            if (i % kProgressBlockSize == 0) {
                print_progress_bar(i / kProgressBlockSize);
            }
            if (result != AC) break;
        }
        fprintf(stderr, "\n");
        if (result != AC) return result;
    }
    return AC;
}

int subtask3(char *exe_path) {
    { // Valid Commands
        fprintf(stderr, "    3-1: Valid commands (Meet:Graduate = 5:1)\n");
        const int kTestTimes = 1000;
        const int kProgressBlockSize = kTestTimes / 100;

        const int kAllowedCmd[] = { MEET, MEET, MEET, MEET, MEET, GRADUATE };
        int result = AC;
        for (int i = 1; i <= kTestTimes; ++i) {
            start_friend_tree(exe_path);
            Tree ref = TreeNew();
            int iterations = random_num(32, 128);
            for (int j = 0; j < iterations; ++j) {
                int command_type;
                if (ref.size < MAX_FRIENDS - 1) {
                    command_type = kAllowedCmd[random_num(0, 5)];
                }
                else {
                    command_type = GRADUATE;
                }
                int iter_result;
                if (command_type == MEET) {
                    iter_result = meet_valid_command(&ref);
                }
                else if (command_type == GRADUATE) {
                    iter_result = graduate_valid_command(&ref, 0.01);
                }
                else {
                    fprintf(stderr, "\ncommand_type: %d, ", command_type);
                    ERR_EXIT("invalid command type in subtask 3");
                }
                if (iter_result != AC || ref.size <= 1) {
                    result = iter_result;
                    break;
                }
            }
            TreeDelete(&ref);
            wait_child();
            if (i % kProgressBlockSize == 0) {
                print_progress_bar(i / kProgressBlockSize);
            }
            if (result != AC) break;
        }
        fprintf(stderr, "\n");
        if (result != AC) return result;
    }
    { // Invalid Commands
        fprintf(stderr, "    3-2: Mix valid/invalid commands (Meet:Graduate = 5:1)\n");
        const int kTestTimes = 1000;
        const int kProgressBlockSize = kTestTimes / 100;

        const int kAllowedCmd[] = { MEET, MEET, MEET, MEET, MEET, GRADUATE };
        int result = AC;
        for (int i = 1; i <= kTestTimes; ++i) {
            start_friend_tree(exe_path);
            Tree ref = TreeNew();
            int iterations = random_num(64, 512);
            for (int j = 0; j < iterations; ++j) {
                int command_type, valid = j <= 48 ? valid : random_num(0, 1);
                if (ref.size == 1) {
                    command_type = MEET;
                }
                else if (ref.size < MAX_FRIENDS - 1 || !valid) {
                    command_type = kAllowedCmd[random_num(0, 5)];
                }
                else {
                    command_type = GRADUATE;
                }
                int iter_result;
                if (command_type == MEET) {
                    if (valid) iter_result = meet_valid_command(&ref);
                    else iter_result = meet_invalid_command(&ref);
                }
                else if (command_type == GRADUATE) {
                    if (valid) iter_result = graduate_valid_command(&ref, 0.01);
                    else iter_result = graduate_invalid_command(&ref);
                }
                else {
                    fprintf(stderr, "\ncommand_type: %d, ", command_type);
                    ERR_EXIT("invalid command type in subtask 3");
                }
                if (iter_result != AC || ref.size <= 1) {
                    result = iter_result;
                    break;
                }
            }
            TreeDelete(&ref);
            wait_child();
            if (i % kProgressBlockSize == 0) {
                print_progress_bar(i / kProgressBlockSize);
            }
            if (result != AC) break;
        }
        fprintf(stderr, "\n");
        if (result != AC) return result;
    }
    return AC;
}

int subtask4(char *exe_path) {
    { // Valid Commands
        fprintf(stderr, "    4-1: Valid commands (Meet:Check:Adopt = 1:1:1)\n");
        const int kTestTimes = 1000;
        const int kProgressBlockSize = kTestTimes / 100;

        const int kAllowedCmd[] = { MEET, CHECK, ADOPT };
        int result = AC;
        for (int i = 1; i <= kTestTimes; ++i) {
            start_friend_tree(exe_path);
            Tree ref = TreeNew();
            int iterations = random_num(32, 128);
            for (int j = 0; j < iterations; ++j) {
                int command_type;
                if (ref.size < 3) {
                    command_type = kAllowedCmd[random_num(0, 1)];
                }
                else if (ref.size < MAX_FRIENDS - 1) {
                    command_type = kAllowedCmd[random_num(0, 2)];
                }
                else {
                    command_type = kAllowedCmd[random_num(1, 2)];
                }
                int iter_result;
                if (command_type == MEET) {
                    iter_result = meet_valid_command(&ref);
                }
                else if (command_type == CHECK) {
                    iter_result = check_valid_command(&ref);
                }
                else if (command_type == ADOPT) {
                    iter_result = adopt_valid_command(&ref);
                }
                else ERR_EXIT("invalid command type in subtask 4");
                if (iter_result != AC) {
                    result = iter_result;
                    break;
                }
            }
            TreeDelete(&ref);
            wait_child();
            if (i % kProgressBlockSize == 0) {
                print_progress_bar(i / kProgressBlockSize);
            }
            if (result != AC) break;
        }
        fprintf(stderr, "\n");
        if (result != AC) return result;
    }
    { // Mixed
        fprintf(stderr, "    4-2: Mixed valid/invalid commands (Meet:Check:Adopt = 1:1:1)\n");
        const int kTestTimes = 1000;
        const int kProgressBlockSize = kTestTimes / 100;

        const int kAllowedCmd[] = { MEET, CHECK, ADOPT };
        int result = AC;
        for (int i = 1; i <= kTestTimes; ++i) {
            start_friend_tree(exe_path);
            Tree ref = TreeNew();
            int iterations = random_num(64, 512);
            for (int j = 0; j < iterations; ++j) {
                int command_type, valid = random_num(0, 1);
                if (ref.size < 3) {
                    command_type = kAllowedCmd[random_num(0, 1)];
                }
                else if (ref.size < MAX_FRIENDS - 1 || !valid) {
                    command_type = kAllowedCmd[random_num(0, 2)];
                }
                else {
                    command_type = kAllowedCmd[random_num(1, 2)];
                }
                int iter_result;
                if (command_type == MEET) {
                    if (valid) iter_result = meet_valid_command(&ref);
                    else iter_result = meet_invalid_command(&ref);
                }
                else if (command_type == CHECK) {
                    if (valid) iter_result = check_valid_command(&ref);
                    else iter_result = check_invalid_command(&ref);
                }
                else if (command_type == ADOPT) {
                    if (valid) iter_result = adopt_valid_command(&ref);
                    else iter_result = adopt_invalid_command(&ref);
                }
                else ERR_EXIT("invalid command type in subtask 4");
                if (iter_result != AC) {
                    result = iter_result;
                    break;
                }
            }
            TreeDelete(&ref);
            wait_child();
            if (i % kProgressBlockSize == 0) {
                print_progress_bar(i / kProgressBlockSize);
            }
            if (result != AC) break;
        }
        fprintf(stderr, "\n");
        if (result != AC) return result;
    }
    return AC;
}

int subtask5(char *exe_path) {
    { // Valid Commands
        fprintf(stderr, "    5-1: Valid commands (Meet:Check:Graduate:Adopt = 3:3:1:3)\n");
        const int kTestTimes = 1000;
        const int kProgressBlockSize = kTestTimes / 100;

        const int kAllowedCmd[] = { 
            MEET, MEET, MEET, 
            CHECK, CHECK, CHECK, 
            GRADUATE,
            ADOPT, ADOPT, ADOPT
        };
        int result = AC;
        for (int i = 1; i <= kTestTimes; ++i) {
            start_friend_tree(exe_path);
            Tree ref = TreeNew();
            int iterations = random_num(64, 256);
            for (int j = 0; j < iterations; ++j) {
                int command_type;
                if (ref.size < 3) {
                    command_type = kAllowedCmd[random_num(0, 5)];
                }
                else if (ref.size < MAX_FRIENDS - 1) {
                    command_type = kAllowedCmd[random_num(0, 9)];
                }
                else {
                    command_type = kAllowedCmd[random_num(3, 9)];
                }
                int iter_result;
                if (command_type == MEET) {
                    iter_result = meet_valid_command(&ref);
                }
                else if (command_type == CHECK) {
                    iter_result = check_valid_command(&ref);
                }
                else if (command_type == ADOPT) {
                    iter_result = adopt_valid_command(&ref);
                }
                else if (command_type == GRADUATE) {
                    iter_result = graduate_valid_command(&ref, 0.01);
                }
                else ERR_EXIT("invalid command type in subtask 5");
                if (iter_result != AC || ref.size <= 1) {
                    result = iter_result;
                    break;
                }
            }
            TreeDelete(&ref);
            wait_child();
            if (i % kProgressBlockSize == 0) {
                print_progress_bar(i / kProgressBlockSize);
            }
            if (result != AC) break;
        }
        fprintf(stderr, "\n");
        if (result != AC) return result;
    }
    { // Mixed
        fprintf(stderr, "    5-2: Mixed valid/invalid commands (Meet:Check:Graduate:Adopt = 3:3:1:3)\n");
        const int kTestTimes = 1000;
        const int kProgressBlockSize = kTestTimes / 100;

        const int kAllowedCmd[] = { 
            MEET, MEET, MEET, 
            CHECK, CHECK, CHECK, 
            GRADUATE,
            ADOPT, ADOPT, ADOPT
        };
        int result = AC;
        for (int i = 1; i <= kTestTimes; ++i) {
            start_friend_tree(exe_path);
            Tree ref = TreeNew();
            int iterations = random_num(64, 512);
            for (int j = 0; j < iterations; ++j) {
                int command_type, valid = random_num(0, 1);
                if (ref.size < 3) {
                    command_type = kAllowedCmd[random_num(0, 2)];
                }
                else if (ref.size < MAX_FRIENDS - 1 || !valid) {
                    command_type = kAllowedCmd[random_num(0, 9)];
                }
                else {
                    command_type = kAllowedCmd[random_num(3, 9)];
                }
                int iter_result;
                if (command_type == MEET) {
                    if (valid || ref.size == 1) iter_result = meet_valid_command(&ref);
                    else iter_result = meet_invalid_command(&ref);
                }
                else if (command_type == CHECK) {
                    if (valid) iter_result = check_valid_command(&ref);
                    else iter_result = check_invalid_command(&ref);
                }
                else if (command_type == ADOPT) {
                    if (valid) iter_result = adopt_valid_command(&ref);
                    else iter_result = adopt_invalid_command(&ref);
                }
                else if (command_type == GRADUATE) {
                    if (valid) iter_result = graduate_valid_command(&ref, 0.01);
                    else iter_result = graduate_invalid_command(&ref);
                }
                else ERR_EXIT("invalid command type in subtask 5");
                if (iter_result != AC || ref.size <= 1) {
                    result = iter_result;
                    break;
                }
            }
            TreeDelete(&ref);
            wait_child();
            if (i % kProgressBlockSize == 0) {
                print_progress_bar(i / kProgressBlockSize);
            }
            if (result != AC) break;
        }
        fprintf(stderr, "\n");
        if (result != AC) return result;
    }
    return AC;
}

int subtask6(char *exe_path) {
    return AC;
}

int subtask7(char *exe_path) {
    return AC;
}

void random_walk(int subtask, char *exe_path) {
    fprintf(stderr, "Test subtask %d\n", subtask);
    int result = -1;
    switch (subtask) {
        case 1: result = subtask1(exe_path); break;
        case 2: result = subtask2(exe_path); break;
        case 3: result = subtask3(exe_path); break;
        case 4: result = subtask4(exe_path); break;
        case 5: result = subtask5(exe_path); break;
        case 6: result = subtask6(exe_path); break;
        case 7: result = subtask7(exe_path); break;
        default: ERR_EXIT("This program is incorrect");
    }
    switch (result) {
    case AC:
        fprintf(stderr, "Subtask %d:  \e[1;32mAC\e[0m\n", subtask);
        break;
    case WA:
        fprintf(stderr, "Subtask %d:  \e[1;31mWA\e[0m\n", subtask);
        break;
    case TLE:
        fprintf(stderr, "Subtask %d: \e[1;34mTLE\e[0m\n", subtask);
        break;
    case RE:
        fprintf(stderr, "Subtask %d:  \e[1;35mRE\e[0m\n", subtask);
        break;
    default:
        ERR_EXIT("Find unknown result");
    }
    fprintf(stderr, "\n");
}

void wait_child() {
    if (child_pid != 0) {
        char cmd[MAX_PATH_LEN * 2 + 128] = {0};
        snprintf(cmd, MAX_PATH_LEN * 2 + 128, "pkill friend -u %s", username);
        int result = system(cmd);
        if (result < 0) {
            fprintf(stderr, "fail to kill friends, please do it manually if they still exist\n");
            kill(getpid(), SIGINT);
        }
        if (WIFSIGNALED(result)) {
            kill(getpid(), SIGINT);
        }
        else {
            kill(child_pid, SIGINT);
            waitpid(child_pid, NULL, 0);
            child_pid = 0;
        }
    }
    int fd = open("Adopt.fifo", O_RDWR);
    if (fd >= 0) {
        unlink("Adopt.fifo");
    }
}