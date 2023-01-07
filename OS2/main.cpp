#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<iostream>
#include<string>
#include<sstream>
#include<vector>
using namespace std;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

extern "C" {
void asm_print(const char *, const int);
}


int  BytsPerSec;	//每扇区字节数
int  SecPerClus;	//每簇扇区数
int  RsvdSecCnt;	//Boot记录占用的扇区数
int  NumFATs;	//FAT表个数
int  RootEntCnt;	//根目录最大文件数
int  FATSz;	//FAT扇区数
int fatBase;
int fileRootBase;
int dataBase;
int BytsPerClus;
#pragma pack (1) /*指定按1字节对齐*/



//偏移11个字节
struct BPB {
    u16  BPB_BytsPerSec;	//每扇区字节数
    u8   BPB_SecPerClus;	//每簇扇区数
    u16  BPB_RsvdSecCnt;	//Boot记录占用的扇区数
    u8   BPB_NumFATs;	//FAT表个数
    u16  BPB_RootEntCnt;	//根目录最大文件数
    u16  BPB_TotSec16;
    u8   BPB_Media;
    u16  BPB_FATSz16;	//FAT扇区数
    u16  BPB_SecPerTrk;
    u16  BPB_NumHeads;
    u32  BPB_HiddSec;
    u32  BPB_TotSec32;	//如果BPB_FATSz16为0，该值为FAT扇区数
};

struct RootEntry {
	char DIR_Name[11];
	u8   DIR_Attr;		//文件属性
	char reserved[10];
	u16  DIR_WrtTime;
	u16  DIR_WrtDate;
	u16  DIR_FstClus;	//开始簇号
	u32  DIR_FileSize;
};
#pragma pack ()

class Node {//链表的node类
public:
    string name;		//名字
    vector<Node *> next;	//下一级目录的Node数组
    string path;			//记录path，便于打印操作
    u32 FileSize;			//文件大小
    bool isfile = false;		//是文件还是目录
    bool isval = true;			//用于标记是否是.和..
    int dir_count = 0;			//记录下一级有多少目录
    int file_count = 0;			//记录下一级有多少文件
    char *content = new char[10000];		//存放文件内容
    Node* parent;
};


//声明所有函数
void myPrint(const char* p);
void RetrieveContent(FILE * fat12, int startClus, Node* son);
int  getFATValue(FILE * fat12, int num);
void createNode(Node *p);
void readChildren(FILE * fat12, int startClus, Node *father);

int availableDir_Name(struct RootEntry* rootEntry_ptr){
    int j;
    int boolean = 0;
    for (j = 0; j < 11; j++) {
        if (!(((rootEntry_ptr->DIR_Name[j] >= 48) && (rootEntry_ptr->DIR_Name[j] <= 57)) ||
              ((rootEntry_ptr->DIR_Name[j] >= 65) && (rootEntry_ptr->DIR_Name[j] <= 90)) ||
              ((rootEntry_ptr->DIR_Name[j] >= 97) && (rootEntry_ptr->DIR_Name[j] <= 122)) ||
              (rootEntry_ptr->DIR_Name[j] == ' '))) {
            boolean = 1;	//非英文及数字、空格
            break;
        }
    }
    return boolean;
}

void ParentAndSon(Node* father,Node* son,string realName,struct RootEntry* rootEntry_ptr){
    father->next.push_back(son);  //存到father的next数组中
    son->name = realName;
    son->FileSize = rootEntry_ptr->DIR_FileSize;
    son->isfile = true;
    son->path = father->path + realName + "/";
    son->parent = father;
    father->file_count++;
}

void easyParentAndSon(Node* father,Node* son, string tempName){
    father->next.push_back(son);
    son->name = tempName;
    son->parent = father;
    son->path = father->path + tempName + "/";
    father->dir_count++;
}


void init(FILE * fat12, struct RootEntry* rootEntry_ptr, Node *father) {
    int base = (RsvdSecCnt + NumFATs * FATSz) * BytsPerSec;	//根目录首字节的偏移数
    int check;
    char realName[12];	//暂存文件名

    //依次处理根目录中的各个条目
    int i;
    for (i = 0; i < RootEntCnt; i++) {

        check = fseek(fat12, base, SEEK_SET);
        if (check == -1)
            myPrint("fseek in printFiles failed!\n");

        check = fread(rootEntry_ptr, 1, 32, fat12);
        if (check != 32)
            myPrint("fread in printFiles failed!\n");

        base += 32;

        if (rootEntry_ptr->DIR_Name[0] == '\0') continue;	//空条目不输出

        //过滤非目标文件

        if (availableDir_Name(rootEntry_ptr) == 1) continue;	//非目标文件不输出

        int k;   //名字的处理
        if ((rootEntry_ptr->DIR_Attr & 0x10) == 0) {
            //文件
            int tempLong = -1;
            for (k = 0; k < 11; k++) {
                if (rootEntry_ptr->DIR_Name[k] != ' ') {
                    tempLong++;
                    realName[tempLong] = rootEntry_ptr->DIR_Name[k];
                }
                else {
                    tempLong++;
                    realName[tempLong] = '.';
                    while (rootEntry_ptr->DIR_Name[k] == ' ') k++;
                    k--;
                }
            }
            tempLong++;
            realName[tempLong] = '\0';	//到此为止，把文件名提取出来放到了realName里
            Node *son = new Node();   //新建该文件的节点
            ParentAndSon(father,son,realName,rootEntry_ptr);
            RetrieveContent(fat12, rootEntry_ptr->DIR_FstClus, son);//读取文件的内容
        }
        else {
            //目录
            int tempLong = -1;
            for (k = 0; k < 11; k++) {
                if (rootEntry_ptr->DIR_Name[k] != ' ') {
                    tempLong++;
                    realName[tempLong] = rootEntry_ptr->DIR_Name[k];
                }
                else {
                    tempLong++;
                    realName[tempLong] = '\0';
                    break;
                }
            }	//到此为止，把目录名提取出来放到了realName
            Node *son = new Node();
            easyParentAndSon(father,son,realName);
            createNode(son);
            //输出目录及子文件
            readChildren(fat12, rootEntry_ptr->DIR_FstClus, son);  //读取目录的内容
        }
    }
}

void readChildren(FILE * fat12, int startClus, Node *father) {
    //数据区的第一个簇（即2号簇）的偏移字节
    int dataBase = BytsPerSec * (RsvdSecCnt + FATSz * NumFATs + (RootEntCnt * 32 + BytsPerSec - 1) / BytsPerSec);
    int currentClus = startClus;
    int value = 0;//value用来查看是否存在多个簇（查FAT表）
    while (value < 0xFF8) {
        value = getFATValue(fat12, currentClus);//查FAT表获取下一个簇号
        if (value == 0xFF7) {
            myPrint("坏簇，读取失败!\n");
            break;
        }
        int startByte = dataBase + (currentClus - 2)*SecPerClus*BytsPerSec;
        int check;
        int count = SecPerClus * BytsPerSec;	//每簇的字节数
        int loop = 0;
        while (loop < count) {
            int i;

            RootEntry sonEntry;//读取目录项
            RootEntry *sonEntryP = &sonEntry;
            check = fseek(fat12, startByte + loop, SEEK_SET);
            if (check == -1)
                myPrint("fseek in printFiles failed!\n");

            check = fread(sonEntryP, 1, 32, fat12);
            if (check != 32)
                myPrint("fread in printFiles failed!\n");//读取完毕
            loop += 32;
            if (sonEntryP->DIR_Name[0] == '\0') {
                continue;
            }	//空条目不输出
            //过滤非目标文件
            if (availableDir_Name(sonEntryP) == 1) {
                continue;
            }


            if ((sonEntryP->DIR_Attr & 0x10) == 0) {
                //文件处理
                char tempName[12];	//暂存替换空格为点后的文件名
                int k;
                int tempLong = -1;
                for (k = 0; k < 11; k++) {
                    if (sonEntryP->DIR_Name[k] != ' ') {
                        tempLong++;
                        tempName[tempLong] = sonEntryP->DIR_Name[k];
                    }
                    else {
                        tempLong++;
                        tempName[tempLong] = '.';
                        while (sonEntryP->DIR_Name[k] == ' ') k++;
                        k--;
                    }
                }
                tempLong++;
                tempName[tempLong] = '\0';	//到此为止，把文件名提取出来放到tempName里
                Node *son = new Node();
                ParentAndSon(father,son,tempName,sonEntryP);
                RetrieveContent(fat12, sonEntryP->DIR_FstClus, son);
            }
            else {
                char tempName[12];
                int count = -1;
                for (int k = 0; k < 11; k++) {
                    if (sonEntryP->DIR_Name[k] != ' ') {
                        count++;
                        tempName[count] = sonEntryP->DIR_Name[k];
                    }
                    else {
                        count++;
                        tempName[count] = '\0';
                    }
                }
                Node *son = new Node();
                easyParentAndSon(father,son,tempName);
                createNode(son);
                readChildren(fat12, sonEntryP->DIR_FstClus, son);
            }
        }
        currentClus = value;//下一个簇
    };
}

void RetrieveContent(FILE *fat12, int startClus, Node *son) {
    int base = BytsPerSec * (RsvdSecCnt + FATSz * NumFATs + (RootEntCnt * 32 + BytsPerSec - 1) / BytsPerSec);
    int currentClus = startClus;
    int value = 0;
    char *pointer = son->content;

    if (startClus == 0) return;

    while (value < 0xFF8) {
        value = getFATValue(fat12, currentClus);
        if (value == 0xFF7) {
            myPrint("坏啦！");
            break;
        }

        int size = SecPerClus * BytsPerSec;
        char *str = (char*)malloc(size);
        char *content = str;
        int startByte = base + (currentClus - 2)*SecPerClus*BytsPerSec;

        fseek(fat12, startByte, SEEK_SET);
        fread(content, 1, size, fat12);

        for (int i = 0; i < size; ++i) {
            *pointer = content[i];
            pointer++;
        }
        free(str);
        currentClus = value;
    }
}

int  getFATValue(FILE * fat12, int num) {
    //FAT1的偏移字节
    int fatBase = RsvdSecCnt * BytsPerSec;
    //FAT项的偏移字节
    int fatPos = fatBase + num * 3 / 2;
    //奇偶FAT项处理方式不同，分类进行处理，从0号FAT项开始
    int type = 0;
    if (num % 2 == 0) {
        type = 0;
    }
    else {
        type = 1;
    }

    //先读出FAT项所在的两个字节
    u16 bytes;
    u16* bytes_ptr = &bytes;
    int check;
    check = fseek(fat12, fatPos, SEEK_SET);
    if (check == -1)
        myPrint("fseek in getFATValue failed!");

    check = fread(bytes_ptr, 1, 2, fat12);
    if (check != 2)
        myPrint("fread in getFATValue failed!");

    //u16为short，结合存储的小尾顺序和FAT项结构可以得到
    //type为0的话，取byte2的低4位和byte1构成的值，type为1的话，取byte2和byte1的高4位构成的值
    if (type == 0) {
        bytes = bytes << 4;   //这里原文错误，原理建议看网上关于FAT表的文章
        return bytes >> 4;
    }
    else {
        return bytes >> 4;
    }
}

void myPrint(const char* p) {
    printf(p,strlen(p));
//    asm_print(p, strlen(p));
}

void createNode(Node *p) {
    Node *q = new Node();
    q->name = ".";
    q->isval = false;
    p->next.push_back(q);
    q->parent=p;
    q = new Node();
    q->name = "..";
    q->isval = false;
    p->next.push_back(q);
    q->parent = p;
}

vector<string> split(string s,const char* delim){
    vector<string> res;
    char* ss = new char[s.length() + 1];
    strcpy(ss, s.c_str());
    char* p;
    p = strtok(ss,delim);
    while(p){
        string str = p;
        res.push_back(str);
        p=strtok(NULL,delim);
    }
    return res;
}

int startsWith(string s, string sub){
    return s.find(sub)==0?1:0;
}

int endsWith(string s,string sub){
    return s.rfind(sub)==(s.length()-sub.length())?1:0;
}

void initBPB(FILE* fat12 , struct BPB* bpb) {
    int check;
    //BPB从偏移11个字节处开始
    check = fseek(fat12,11,SEEK_SET);
    if (check == -1)
        printf("fseek in fillBPB failed!");
    //BPB长度为25字节
    check = fread(bpb,1,25,fat12);
    if (check != 25)
        printf("fread in fillBPB failed!");
    //初始化各个全局变量
    BytsPerSec = bpb->BPB_BytsPerSec;
    SecPerClus = bpb->BPB_SecPerClus;
    RsvdSecCnt = bpb->BPB_RsvdSecCnt;
    NumFATs = bpb->BPB_NumFATs;
    RootEntCnt = bpb->BPB_RootEntCnt;
    if (bpb->BPB_FATSz16 != 0) {
        FATSz = bpb->BPB_FATSz16;
    } else {
        FATSz = bpb->BPB_TotSec32;
    }
    fatBase = RsvdSecCnt * BytsPerSec;
    fileRootBase = (RsvdSecCnt + NumFATs * FATSz) * BytsPerSec; //根目录首字节的偏移数=boot+fat1&2的总字节数
    dataBase = BytsPerSec * (RsvdSecCnt + FATSz * NumFATs + (RootEntCnt * 32 + BytsPerSec - 1) / BytsPerSec);
    BytsPerClus = SecPerClus * BytsPerSec; //每簇的字节数
}

void printLS(Node* root){
    Node* temp = root;
    if(temp->isfile){
        return;
    }
    else {
        string res = temp->path + ":\n";
        myPrint(res.c_str());
        res = "";
        Node *nextNode;
        int length = root->next.size();
        for (int i = 0; i < length; i++) {
            nextNode = temp->next[i];
            if (!nextNode->isfile) {
                res = "\033[31m" + nextNode->name + "\033[0m" + "  ";
                myPrint(res.c_str());
                res = "";
            } else {
                res = nextNode->name + "  ";
                myPrint(res.c_str());
                res = "";
            }
        }
        myPrint("\n");
        for (int j = 0; j < length; j++) {
            if (!(temp->next[j]->name == "." || temp->next[j]->name == ".."))printLS(temp->next[j]);
        }
    }
}

void printLS_L(Node* root){
    Node* temp = root;
    if(temp->isfile){
        return;
    }
    else {
        string res = temp->path + " " + to_string(temp->dir_count) + " " + to_string(temp->file_count) + ":\n";
        myPrint(res.c_str());
        res = "";
        Node *nextNode;
        int length = root->next.size();
        for (int i = 0; i < length; i++) {
            nextNode = temp->next[i];
            if (!nextNode->isfile) {
                if (nextNode->name == "." || nextNode->name == "..") {
                    res = "\033[31m" + nextNode->name + "\033[0m" + "  \n";
                    myPrint(res.c_str());
                    res = "";
                } else {
                    res = "\033[31m" + nextNode->name + "\033[0m" + "  " + to_string(nextNode->dir_count) + " " +
                          to_string(nextNode->file_count) + "\n";
                    myPrint(res.c_str());
                    res = "";
                }
            } else {
                res = nextNode->name + "  " + to_string(nextNode->FileSize) + "\n";
                myPrint(res.c_str());
                res = "";
            }
        }
        res = "\n";
        myPrint(res.c_str());
        res = "";
        for (int j = 0; j < length; j++) {
            if (!(temp->next[j]->name == "." || temp->next[j]->name == "..")) {
                printLS_L(temp->next[j]);
            }
        }
    }
}



void dealLS(string path,bool l,Node* root,string& err,bool containDot){
    if(containDot){
        const char* splitSym1 = "/";
        vector<string> names = split(path,splitSym1);
        Node* current = root;
        for(int i=0;i<names.size();i++){
            if(names[i].compare("") == 0){
                continue;
            }
            else if(names[i].compare(".")==0){
                continue;
            }
            else if(names[i].compare("..") == 0){
                current = current->parent;
            }
            else{
                for(int j=0;j<current->next.size();j++){
                    if (current->next[j]->name.compare(names[i])==0){
                        current = current->next[j];
                        break;
                    }
                    if(j==current->next.size()-1){
                        return;
                    }
                }
            }
        }
        if (current->isfile) { //if it is file
            //error a file
            err = "file";
            return;
        } else {
            err = "no";
            if (l) {
                printLS_L(current);
            } else {
                printLS(current);
            }
            return;
        }
        return;
    }
    else {
        if (root->path.compare(path) == 0) {
            if (root->isfile) { //if it is file
                //error a file
                err = "file";
                return;
            } else {
                err = "no";
                if (l) {
                    printLS_L(root);
                } else {
                    printLS(root);
                }
            }
            return;
        }
        string temp = path.substr(0, root->path.length());
        if (temp.compare(root->path) == 0) {
            for (Node *nextNode: root->next) {
                dealLS(path, l, nextNode, err,false);
            }
        }
    }
}

void dealCat(string path,Node* root,string& err,bool containDot){
    if(containDot){
        const char* splitSym1 = "/";
        vector<string> names = split(path,splitSym1);
        Node* current = root;
        for(int i=0;i<names.size();i++){
            if(names[i].compare("") == 0){
                continue;
            }
            else if(names[i].compare(".")==0){
                continue;
            }
            else if(names[i].compare("..") == 0){
                current = current->parent;
            }
            else{
                for(int j=0;j<current->next.size();j++){
                    if (current->next[j]->name.compare(names[i])==0){
                        current = current->next[j];
                        break;
                    }
                    if(j==current->next.size()-1){
                        return;
                    }
                }
            }
        }
        if (current->isfile) { //if it is file
            //error a file
            err = "no";
            if (current->content[0] != 0) {
                myPrint(current->content);
                myPrint("\n");
                return;
            }

        } else {
            // error not a file
            err = "notfile";
            return;
        }
    }
    else {
        if (path[0] != '/') {
            path = "/" + path;
        }
        if (path.compare(root->path) == 0) {
            if (root->isfile) {
                //right
                err = "no";
                if (root->content[0] != 0) {
                    myPrint(root->content);
                    myPrint("\n");
                    return;
                }
            } else {
                // error not a file
                err = "notfile";
                return;
            }
        }
        string temp = path.substr(0, root->path.length());
        if (temp == root->path) {
            for (Node *nextNode: root->next) {
                dealCat(path, nextNode, err,false);
            }
        }
    }
}


int main() {
    FILE *fat12;
    fat12 = fopen("/Users/harunzzz/Codes/OS/02/a2.img", "rb");
    
    struct BPB bpb_;
    struct BPB* bpb = &bpb_;
    //载入BPB
    initBPB(fat12,bpb);

    Node* root = new Node();
    root->name = "";
    root->path = "/";
    root->parent = root;

    struct RootEntry rootEntry;
	struct RootEntry* rootEntry_ptr = &rootEntry;

    init(fat12, rootEntry_ptr, root);
    
    while(true){
        vector<string> instructions;
        string input;
        getline(cin,input);
        const char* splitSym = " ";
        instructions = split(input,splitSym);
        if(instructions[0].compare("exit") == 0){
            myPrint("退出程序\n");
            fclose(fat12);
            return 0;
        }
        else if (instructions[0].compare("cat") == 0){
            string path;
            path = instructions[1];
            if(!endsWith(path,"/")){
                path = path + "/";
            }
            bool containDot;
            string::size_type idx;
            idx=path.find(".");
            if(idx == string::npos){
                containDot = false;
            }
            else{
                containDot = true;
            }
            string error = "notfind";
            dealCat(path,root,error,containDot);
            if(error.compare("notfind")==0){
                myPrint("无该文件\n");
            }
            else if(error.compare("notfile")==0){
                myPrint("此项目不是文件\n");
            }
            continue;
        }
        else if (instructions[0].compare("ls") == 0){
            bool containL = false;
            string error = "notfind";
            string path = "";
            bool pathAgain = false;
            bool stop = false;
            for(int i=1;i<instructions.size();i++){
                if(startsWith(instructions[i],"-")){
                    if(startsWith(instructions[i],"-l")){
                        containL = true;
                    }
                    else{
                        stop = true;
                        myPrint("错误参数\n");
                    }
                }
                else{
                    if(path.compare("")!=0){
                        pathAgain = true;
                        break;
                    }
                    if(!startsWith(instructions[i],"/")){
                        path = "/" + instructions[i];
                    }
                    else{
                        path = instructions[i];
                    }
                }
            }
            if(pathAgain){
                myPrint("错误：两条路径\n");
                break;
            }
            if(stop){
                break;
            }
            if(path.compare("")==0){
                path = "/";
            }
            bool containDot;
            if(!endsWith(path,"/")){
                path = path + "/";
            }
            string::size_type idx;
            idx=path.find(".");
            if(idx == string::npos){
                containDot = false;
            }
            else{
                containDot = true;
            }
            dealLS(path,containL,root,error,containDot);
            if(error.compare("notfind")==0){
                myPrint("无该路径\n");
            }
            else if(error.compare("file")==0){
                myPrint("此项目是文件\n");
            }
            continue;
        }
        else{
            //output error
            myPrint("未识别指令\n");
            continue;
        }
    }
}
