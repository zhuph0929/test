//
// Created by gy on 23-7-25.
//

#ifdef _WIN32 // 如果是Windows系统
#include "plugin.h"
#include <sys/stat.h>
#include <gio/gio.h>
#include <stdio.h>
#include <pthread.h>
#include <winsock2.h>
#include <setjmp.h>
#include "cJSON.h"

#pragma comment(lib, "ws2_32.lib") // 链接Winsock库

#else // 如果是Linux系统
#include "plugin.h"
#include <sys/stat.h>
#include <gio/gio.h>
#include <stdio.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <setjmp.h>
#include <cJSON.h>
#endif



typedef struct GTYPE GTYPE;

GtkWidget *com_source = NULL;   /* window component: drag source */
GtkWidget *gen_case = NULL;     /* window component: drag destination */
GtkWidget *para_set = NULL;     /* TBD */
GtkWidget *para2_set = NULL;    /* TBD */

GtkWidget *save_btn = NULL;         /* menu component: save button */
GtkWidget *open_btn = NULL;         /* menu component: open button */
GtkWidget *export_btn = NULL;       /* menu component: export button */
GtkWidget *properities_btn = NULL;  /* menu component: properities button */
GtkWidget *import_btn = NULL;       /* menu component: import button */

GtkWidget *run_btn = NULL;  /* menu component: run button */

GtkWidget *toolbar_add_case_btn = NULL;         /* toolbar component: toolbar_add_case button */
GtkWidget *toolbar_run_btn = NULL;              /* toolbar component: run button */
GtkWidget *toolbar_debug_btn = NULL;            /* toolbar component: debug button */
GtkWidget *toolbar_debug_step_btn = NULL;       /* toolbar component: step_debug button */
GtkWidget *toolbar_debug_execute_btn = NULL;    /* toolbar component: debug_execute button */


int com_source_index;   /* index of drag source window in notebook */
int gen_case_index;     /* index of drag destination window in notebook */
int para_set_index;     /* TBD */
int para2_set_index;    /* TBD */

char* current_pwd = NULL;   /* 定位到geany的插件目录 */

GeanyPlugin *tep_geany = NULL;  /* geany application entry */

/**
 * 下方状态栏使用变量
 */
GtkWidget *debug_text = NULL;
GtkTextBuffer *debug_textbuffer = NULL;
GtkTextIter debug_start,debug_end;
GtkTextMark* debug_mark = NULL;

GtkWidget *file_text = NULL;
GtkTextBuffer *file_textbuffer = NULL;
GtkTextIter file_start,file_end;
GtkTextMark* file_mark = NULL;

FunctionInfo *functions = NULL;
int num_functions = 0;
char* drag_component_name = NULL;   //拖动组件名称

/**
 *文件配置
 */
char* python_script_path;
char* python_system_interpreter;
char* import_api_path;
gboolean import_api_path_status = FALSE;
char* python_info_path;
char* python_xml_path;      //python脚本需要的xml路径
char* default_xml_path;
char* default_open_file_path;

gboolean default_xml_is_vaild;

int content_index=0;
int main_index=1;   //控制主循环

GArray *code_array = NULL;
GArray *thread_array = NULL;
GArray *file_array = NULL;

GArray *drag_row_models = NULL;
GArray *drag_row_iter = NULL;
GArray *function_node_name = NULL;

GArray *not_drag_dest = NULL;

GArray *xml_nodes = NULL;
GArray *state = NULL;
GArray *iter_array = NULL;

GArray *debug_point = NULL;

GArray *receive_json_array = NULL;
int receive_json_array_len ;

gpointer executing_row_model;   //正在执行的类
int executing_classId = 0;      //正在执行的类id

GValue value_py ;
GValue value_py_sys_inter ;
GValue value_import_api_path;
GValue value_import_api_status;

guint current_drag_windows = 0;

gboolean execute_api_stastus;
gboolean thread_lock = FALSE;
gboolean gen_case_click_is_row;
gboolean debug_windows_state;
gboolean socket_mess_lock;

int socket_client_id = 0;
int socket_server_id = 0;

int json_num = 0;
int update_thread_id = 0;
gboolean python_scripy_conpleted = FALSE;

GFile *monitor_file;
GFileMonitor *monitor;

GtkTreeIter my_iter;
gpointer my_row_model;
int z = 0;

gboolean shear = FALSE;
gboolean copy = FALSE;

jmp_buf exception_env;

GdkPixbuf *point_pixbuf = NULL;

//debug 弹窗
GtkWidget *debug_window;
GtkWidget *debug_label;

void handle_exception()
{
    printf("发生异常\n");
    longjmp(exception_env, 1);
}

#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

#ifdef _WIN32
#define MKDIR_TEST_PLUGIN mkdir(current_pwd);
#else
#define MKDIR_TEST_PLUGIN mkdir(current_pwd,0700);
#endif

#ifdef _WIN32
#define MKDIR_INFO mkdir(g_strdup_printf("%s%s%s",current_pwd,PATH_SEPARATOR,"info"))
#else
#define MKDIR_INFO mkdir(g_strdup_printf("%s%s%s",current_pwd,PATH_SEPARATOR,"info"),0700)
#endif

#ifdef _WIN32
#define MKDIR_XML mkdir(g_strdup_printf("%s%s%s",current_pwd,PATH_SEPARATOR,"xml"))
#else
#define MKDIR_XML mkdir(g_strdup_printf("%s%s%s",current_pwd,PATH_SEPARATOR,"xml"),0700)
#endif

#ifdef _WIN32
#define GOTO_HTM system(sys_call)
#else
#define GOTO_HTM printf("system call\n")
#endif

#ifdef _WIN32
#define CLIENT_SOCKET SOCKET
#else
#define CLIENT_SOCKET int
#endif

CLIENT_SOCKET global_clientSocket;

#ifdef _WIN32
void* socket_thread(void* arg)
{
    global_clientSocket = INVALID_SOCKET;

    printf("Hello from the thread!\n");
    // 创建一个IPv4 TCP socket
    WSADATA wsaData;
    SOCKET serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    int clientAddrLen = sizeof(clientAddr);
    char buffer[1024];

    // 初始化Winsock库
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("Failed to initialize Winsock\n");
        return 1;
    }

    // 创建套接字
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET)
    {
        printf("Failed to create socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // 设置服务器地址结构
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345); // 服务器监听的端口号
    serverAddr.sin_addr.s_addr = INADDR_ANY; // 监听所有网络接口

    // 绑定套接字
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        printf("Bind failed: %d\n", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // 监听连接请求
    if (listen(serverSocket, 5) == SOCKET_ERROR)
    {
        printf("Listen failed: %d\n", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    printf("Server listening on port 12345...\n");
    while(1)
    {
        // 等待客户端连接
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        global_clientSocket = clientSocket;
        if (clientSocket == INVALID_SOCKET)
        {
            printf("Accept failed: %d\n", WSAGetLastError());
            closesocket(serverSocket);
            WSACleanup();
            return 1;
        }

        printf("Client connected\n");

        char* before_buffer = "";
        // 接收客户端消息
        while (1)
        {
            int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesRead <= 0)
            {
                printf("Connection closed by client\n");
                break;
            }
            buffer[bytesRead] = '\0';

            if(strlen(buffer) >= 1023)
            {
                printf("Message exceeds the length of 1024\n"
                       "Message sent too fast, please resend\n");
                continue;
            }

            printf("Serive received: %s", buffer);
            //debug_textbuff_output(buffer);

            printf("json len = %d\n",strlen(buffer));
            if ( strlen(buffer) <= 2)
            {
                printf("not json\n");
                continue;
            }

            //消息处理
            processString(buffer);

            // 如果接收到"end"，关闭连接
            if (strncmp(buffer, "end", 3) == 0)
            {
                printf("Received 'end'. Closing connection.\n");
                break;
            }
        }
        // 关闭socket
        close(clientSocket);
    }
    close(serverSocket);

    WSACleanup();
    return 0;
}

void sendMessageToClient(const char* message)
{
    if (global_clientSocket != INVALID_SOCKET)
    {
          int bytesSent = send(global_clientSocket, message, strlen(message), 0);
          if (bytesSent == SOCKET_ERROR)
          {
              printf("***socket: Failed to send message: %d\n", WSAGetLastError());
              // 这里可以添加错误处理逻辑，比如关闭连接或者重新连接
          } else
          {
              printf("***socket: Message sent to client: %s\n", message);
          }
    } else
    {
        printf("***socket: No client connected to send message to\n");
        // 这里可以添加逻辑来处理没有连接的情况
    }
}

#else
void* socket_thread(void* arg)
{
    global_clientSocket = -1;

    printf("清除数组中的所有元素!\n");

    // 创建一个IPv4 TCP socket
    int serverSocket;
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    socket_server_id = serverSocket;

    if (serverSocket == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }

    // 设置服务器地址结构体
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345); // 服务器监听端口
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // 绑定socket
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("Bind failed");
        close(serverSocket);
        exit(1);
    }

    // 监听连接
    if (listen(serverSocket, 5) == -1)
    {
        perror("Listen failed");
        close(serverSocket);
        exit(1);
    }

    printf("Server listening on port 12345...\n");

    while(1)
    {
        // 等待客户端连接
        int clientSocket;
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddr, &clientAddrLen);
        global_clientSocket = clientSocket;

        socket_client_id = clientSocket;
        if (clientSocket == -1)
        {
            perror("Accept failed");
            close(serverSocket);
            exit(1);
        }

        printf("Client connected.\n");

        char buffer[1024];
        int recvSize;
        do
        {
            recvSize = recv(clientSocket, buffer, sizeof(buffer), 0);
            while(1)
            {
                if(!socket_mess_lock)
                {
                    socket_mess_lock = TRUE;
                    if (recvSize > 0)
                    {
                        buffer[recvSize] = '\0'; // 添加字符串结束符
//                        if(strlen(buffer) >= 1023)
//                        {
//                            printf("Message exceeds the length of 1024\n"
//                                   "Message sent too fast, please resend\n");
//                            continue;
//                        }
                        printf("Received: %s\n", buffer);
                        if (strcmp(buffer, "end") != 0)
                        {
                            printf("json len = %d\n", strlen(buffer));
                            if (strlen(buffer) <= 2)
                            {
                                printf("not json\n");
                                continue;
                            }
                            processString(buffer);
                        }
                    }
                    break;
                }
            }
            socket_mess_lock = FALSE;
        }while (strcmp(buffer, "end") != 0 && recvSize > 0);
        // 关闭客户端socket
        close(clientSocket);
    }
    printf("Client disconnected.\n");

}

void sendMessageToClient(const char *message)
{
    if (global_clientSocket != -1)
    {
        while (1)
        {
            if (!socket_mess_lock)
            {
                socket_mess_lock = TRUE;
                int messageLength = strlen(message);
                int bytesSent = send(global_clientSocket, message, messageLength, 0);

                if (bytesSent == -1)
                {
                    perror("Send failed");
                } else
                {
                    printf("Sent message to client: %s\n", message);
                }
                socket_mess_lock = FALSE;
                break; // After sending the message, exit the loop
            }
        }
    } else
    {
        printf("No client connected to send message to\n");
        // Logic to handle no connection can be added here
    }
}
#endif

/**
 * 处理json
 * @param json_str
 */
void parse_json(const char *json_str)
{
    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        return;
    }

    const cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");
    if (cJSON_IsNumber(id))
    {
        printf("[cJSON] id: %d\n", id->valueint);
    }

    const cJSON *result = cJSON_GetObjectItemCaseSensitive(json, "result");
    if (cJSON_IsBool(result))
    {
        printf("[cJSON] result: %s\n", cJSON_IsTrue(result) ? "true" : "false");
    }

    const cJSON *message = cJSON_GetObjectItemCaseSensitive(json, "message");
    if (cJSON_IsString(message) && (message->valuestring != NULL))
    {
        printf("[cJSON] message: \"%s\"\n", message->valuestring);
    }

    const cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (cJSON_IsString(type) && (type->valuestring != NULL))
    {
        printf("[cJSON] type: \"%s\"\n", type->valuestring);
    }

    cJSON_Delete(json);
}

/**
 * 遍历全部json键值对
 */
char* print_json_key_value_pairs(const char *json_str)
{
    char* result = "";
    GString *ret = g_string_new("");
    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        return "";
    }

    // 迭代 JSON 对象的每一个键值对
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, json)
    {
        if (cJSON_IsString(item) && (item->valuestring != NULL))
        {
            printf("%s: %s\n", item->string, item->valuestring);
            g_string_append(ret, g_strdup_printf("%s: %s\n", item->string, item->valuestring));
        } else if (cJSON_IsNumber(item))
        {
            printf("%s: %d\n", item->string, item->valueint);
            g_string_append(ret, g_strdup_printf("%s: %d\n", item->string, item->valueint));
        } else if (cJSON_IsBool(item))
        {
            printf("%s: %s\n", item->string, cJSON_IsTrue(item) ? "true" : "false");
            g_string_append(ret, g_strdup_printf("%s: %s\n", item->string, cJSON_IsTrue(item) ? "true" : "false"));
        } else
        {
            // 你可以添加更多的类型检查和打印逻辑
        }

    }
    cJSON_Delete(json); // 清理 cJSON 对象
    result = g_string_free(ret, FALSE);  // 获取结果字符串，并保留内存
    return result;
}

void clear_container(GtkWidget *widget, gpointer data)
{
    gtk_widget_destroy(widget);
}

void create_window_with_json(const char *json_str)
{
    // 首先清除 debug_window 中的所有内容
    gtk_container_foreach(GTK_CONTAINER(debug_window), clear_container, NULL);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5); // 设置行间距

    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        return;
    }

    int row = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, json)
    {
        GString *ret = g_string_new("");
        if (cJSON_IsString(item) && (item->valuestring != NULL))
        {
            g_string_append_printf(ret, "%s: %s", item->string, item->valuestring);
        } else if (cJSON_IsNumber(item))
        {
            g_string_append_printf(ret, "%s: %d", item->string, item->valueint);
        } else if (cJSON_IsBool(item))
        {
            g_string_append_printf(ret, "%s: %s", item->string, cJSON_IsTrue(item) ? "true" : "false");
        }
        GtkWidget *label = gtk_label_new(ret->str);
        gtk_widget_set_halign(label, GTK_ALIGN_START); // 水平对齐
        gtk_widget_set_valign(label, GTK_ALIGN_CENTER); // 垂直对齐
        gtk_label_set_xalign(GTK_LABEL(label), 0.0); // 文本的左对齐
        gtk_widget_set_margin_start(label, 10);
        gtk_widget_set_margin_end(label, 10);
        gtk_grid_attach(GTK_GRID(grid), label, 0, row++, 1, 1);

        if (item->next)
        {
            GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
            gtk_grid_attach(GTK_GRID(grid), separator, 0, row++, 1, 1);
        }
        g_string_free(ret, TRUE);
    }

    cJSON_Delete(json);

    gtk_container_add(GTK_CONTAINER(debug_window), grid);
    gtk_widget_show_all(debug_window);
}

/**
 * 处理接受字符串
 * @param str
 */
void processString(char *str)
{
    char *start = str;
    char *end;

    while (*start)
    {
        // 查找以"{"开头的子字符串
        start = strchr(start, '{');
        if (!start)
        {
            // 如果找不到"{"，退出循环
            break;
         }

        // 查找以"}"结尾的子字符串
        end = strchr(start, '}');
        if (!end)
        {
            // 如果找不到"}"，跳过当前子字符串
            break;
        }

        // 检查截取的子字符串长度是否大于50
        int length = end - start + 1;
        // 处理满足条件的子字符串
        char* receive = g_strdup_printf("%.*s",length-2, start+1);
        printf("mess len %d\n",strlen(receive));
        printf("Valid Message : %s\n", receive); // 打印或执行其他操作
        //parse_json(receive);
        message_allocation(receive);

        // 移动开始位置，继续寻找下一个子字符串
        start = end + 1;
    }
}

void properities_file_save()
{
    char* contents= g_strdup_printf("python_script_path=%s\n"
                                    "python_system_interpreter=%s\n"
                                    "import_api_path=%s;"
                                    "status=%s\n"
                                    "default_open_file_path=%s",
                                    g_value_get_string(&value_py),
                                    g_value_get_string(&value_py_sys_inter),
                                    g_value_get_string(&value_import_api_path),
                                    g_value_get_string(&value_import_api_status),
                                    default_open_file_path);

    char* properity_file = g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"properities.txt");
    printf("content : \n%s",contents);
    // 将修改后的内容写回到文件中
    if (g_file_set_contents(properity_file, contents, -1, NULL)) {
        debug_textbuff_output("\n配置文件写入完成\n");
        g_print("%s\n", properity_file);
        g_print("File saved successfully.\n");
        path_init();
    }else
    {
        g_print("File saved fail!  \n");
    }
}

char* unicode_escape_to_utf8(const char* input)
{
    char* output = malloc(strlen(input) + 1);
    char* out = output;
    const char* in = input;

    while (*in) {
        if (*in == '\\' && *(in + 1) == 'u') {
            // 解析Unicode转义序列
            unsigned int codepoint;
            if (sscanf(in + 2, "%4x", &codepoint) == 1) {
                // 将Unicode码点转换为UTF-8编码
                if (codepoint <= 0x7F) {
                    *out++ = (char)codepoint;
                } else if (codepoint <= 0x7FF) {
                    *out++ = (char)(0xC0 | (codepoint >> 6));
                    *out++ = (char)(0x80 | (codepoint & 0x3F));
                } else if (codepoint <= 0xFFFF) {
                    *out++ = (char)(0xE0 | (codepoint >> 12));
                    *out++ = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                    *out++ = (char)(0x80 | (codepoint & 0x3F));
                }
                in += 6; // 跳过Unicode转义序列
            } else {
                *out++ = *in++; // 不是有效的Unicode转义序列，原样复制
            }
        } else {
            *out++ = *in++;
        }
    }

    *out = '\0';
    return output;
}

char* insertMyBeforeFlag(char* input, const char* dest)
{
    if (strstr(input, dest) == NULL)
    {
        return g_strdup(input);  // 如果没有找到dest，返回输入的副本
    }

    char *result = NULL;
    GString *gstring = g_string_new(NULL);  // 创建一个动态字符串
    char *flag = g_strdup_printf("</%s",dest);
    char *dest1 = g_strdup_printf("></%s>",dest);
    char *dest2 = g_strdup_printf("<%s",dest);
    char **content = g_strsplit(input, flag, -1);

    for (int i = 0; content[i] != NULL; ++i)
    {
        if (i > 0)
        {
            char **parms = g_strsplit(content[i], ">", 2);
            g_string_append(gstring, dest2);  // 在不是第一部分的内容前加上flag
            g_string_append(gstring, parms[0]);
            g_string_append(gstring, dest1);
            printf("%d\n",i);
            if(g_strcmp0(parms[1],"") == 0)
            {
                printf("%s\n","null");
            }else
            {
                g_string_append(gstring, parms[1]);
                printf("%s\n","not null");
            }
            printf("%s\n",parms[1]);
        }else
        {
            g_string_append(gstring, content[i]);  // 添加分割的部分
        }
    }
    result = g_string_free(gstring, FALSE);  // 获取结果字符串，并保留内存
    g_strfreev(content);  // 释放分割的字符串数组

    return result;  // 返回结果字符串
}

void* system_thread(void* arg)
{
    char* mode = (char*)arg;
    printf("system_thread : mode = %s\n",mode);

    printf("颜色初始化遍历 开始 ！\n");
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case));
    gpointer row_model;
    GtkTreeIter current_iter;
    gboolean valid = gtk_tree_model_get_iter_first(model, &current_iter);

    RECEIVE_JSON receiveJson = {0,"white",""};

    while (valid)
    {
        gtk_tree_model_get(model, &current_iter, COL_ROW_MODEL, &row_model, -1);
        GString *name = TEP_BASIC_COMPONENT(row_model)->action_name_renderer(row_model);

        int classId = TEP_BASIC_COMPONENT(row_model)->classId;
        printf("%s %d \n",name->str,classId);
        update_class_property(row_model,receiveJson);

        find_chilren_node(current_iter,receiveJson);
        valid = gtk_tree_model_iter_next(model, &current_iter);
    }
    printf("颜色初始化遍历 完成 \n");

    //调用python脚本
    printf("调用python函数 ---\n");
    gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(tep_geany->geany_data->main_widgets->notebook));
    python_info_path = g_array_index(file_array,File,page).path;
    gchar *code = tep_get_program_code(gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case)));

    call_python_script(python_script_path,"main",code,mode);

    printf("python函数执行完成 ---\n");
//
//    if(update_thread_id == 0)
//    {
//        printf("creating thread3: %d\n");
//        pthread_t thread3;
//        int result3 = pthread_create(&thread3, NULL, update_thread, NULL);
//        if (result3 != 0) {
//            printf("Error creating thread3: %d\n", result3);
//        }
//    }
    python_scripy_conpleted = TRUE;
    pthread_exit(NULL);
    json_num = 0;

}

/**
 * 删除指定字符
 * @param str
 * @param flag
 */
void removeSpacesAndNewlines(char *str , char flag)
{
    int i, j = 0;
    for (i = 0; str[i]; i++)
    {
        if ( str[i] != flag)
        {
            str[j++] = str[i];
        }
    }
    str[j] = '\0';
}

/**
 * 删除指定字符串
 * @param str
 * @param sub
 */
void removeSubstring(char *str, const char *sub)
{
    if (str == NULL || sub == NULL || *sub == '\0')
    {
        return; // 处理空字符串或空子字符串
    }

    int len = strlen(sub);
    char *next;

    while ((next = strstr(str, sub)) != NULL)
    {
        memmove(next, next + len, strlen(next + len) + 1);
        str = next + 1; // 更新搜索起点以避免重复搜索相同位置
    }
}

/**
 * 创建带图片的按钮
 */
GtkWidget* new_pixbuf_button(char* name,char* pix_path,int width,int height)
{
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(pix_path, NULL);
    pixbuf = gdk_pixbuf_scale_simple(pixbuf,width, height, GDK_INTERP_BILINEAR);
    GtkWidget *icon = gtk_image_new_from_pixbuf(pixbuf);
    GtkWidget *button = gtk_tool_button_new(icon, name);
    gtk_widget_set_tooltip_text(button,name);
    gtk_widget_show_all(button);
    gtk_tool_button_set_use_underline(GTK_TOOL_BUTTON(button), TRUE);

    return button;
}

/**
 *输出内容到textbuffer，并滚动窗口到标记位置
 */
void debug_textbuff_output(char* mess)
{
    gtk_text_buffer_get_end_iter(debug_textbuffer,&debug_end);
    gtk_text_buffer_insert(debug_textbuffer,&debug_end,mess,-1);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(debug_text), debug_mark, 0.0, TRUE, 0.0, 1.0);
}

void file_textbuff_output(char* mess)
{
    gtk_text_buffer_get_end_iter(file_textbuffer,&file_end);
    gtk_text_buffer_insert(file_textbuffer,&file_end,mess,-1);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(file_text), file_mark, 0.0, TRUE, 0.0, 1.0);
}

gboolean isValidJSON(const char* jsonString)
{
    // JSON格式的基本结构是一个对象，以大括号开始和结束
    // 所以可以检查字符串的第一个字符是否为'{'
    if (jsonString[0] != '{')
    {
        return FALSE;
    }

    // 检查字符串的最后一个字符是否为'}'
    size_t len = strlen(jsonString);
    if (jsonString[len - 1] != '}')
    {
        return FALSE;
    }

    return TRUE;
}

//回调函数，用于处理文件变化事件
void file_changed(GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type,
                  gpointer user_data,int* pindex)
{
    printf("文件发现变化\n");
    // 检查事件类型是否为文件被创建或修改
    if (event_type == G_FILE_MONITOR_EVENT_CREATED || event_type == G_FILE_MONITOR_EVENT_CHANGED)
    {
        printf("文件内容增加\n");
        // 读取文件内容
        gchar *contents = NULL;
        gsize length = 0;
        GError *error = NULL;
        gboolean success = g_file_load_contents(file, NULL, &contents, &length, NULL, &error);
        char** contents_split= g_strsplit(contents,"\n",-1);
        int *current_page_index = (int *)user_data;

        if (success)
        {
            printf("成功读取文件\n");
            char* mess;
            while(contents_split[content_index] != NULL)
            {
                mess = g_strdup_printf("%s\n", contents_split[content_index]);
                mess = unicode_escape_to_utf8(mess);
                file_textbuff_output(mess);

                printf("%d row ,content ,%s\n",content_index,mess);

                //update_cell_color(contents_split[content_index]);

                content_index++;
            }
            content_index--;
            //g_free(mess);
        }
        else
        {
            g_print("Failed to read file: %s\n", error->message);
            g_error_free(error);
        }
        g_free(contents_split);
        g_free(contents);
    }
}

//监听文件
void create_file_monitor(int current_page_index)
{
    char* file_path = g_array_index(file_array,File,current_page_index).path;
    // 创建文件监视器
    monitor_file = g_file_new_for_path(file_path);
    monitor = g_file_monitor_file(monitor_file, G_FILE_MONITOR_NONE, NULL, NULL);
    // 连接文件变化事件的回调函数
    g_signal_connect(monitor, "changed", G_CALLBACK(file_changed), &current_page_index);
    // 运行主循环
    printf("创建主循环成功,正在监听文件 %s---\n",file_path);
    main_index++;
}

int create_info_file(int cur_page_index)
{
    // 获取当前时间的秒数
    time_t currentTime = time(NULL);
    // 将时间转换为本地时间
    struct tm *localTime = localtime(&currentTime);
    // 提取秒数
    int year = localTime->tm_year + 1900;
    int mon = localTime->tm_mon + 1;
    int day = localTime->tm_mday;
    int hour = localTime->tm_hour;
    int min = localTime->tm_min;
    int seconds = localTime->tm_sec;
    char *info_filename = NULL;
    char *xml_filename = NULL;
    info_filename= g_strdup_printf("info_%d-%d-%d_%d%d%d.txt", year,mon,day,hour,min,seconds);
    xml_filename= g_strdup_printf("xml_%d-%d-%d_%d%d%d.txt", year,mon,day,hour,min,seconds);
    printf("当前时间 %d-%d-%d  %d:%d:%d\n", year,mon,day,hour,min,seconds);

/**-------------------------------------------------create info------------------------------------------------------------------**/

    // 判断是否存在info文件夹
    struct stat st1;
    if (stat(g_strdup_printf("%s%s%s",current_pwd,PATH_SEPARATOR,"info"), &st1) == -1)
    {
        // 如果不存在，则创建info文件夹
        MKDIR_INFO;
        printf("成功创建info文件夹\n");
    } else
    {
        printf("info文件夹已存在\n"
               "path = %s",g_strdup_printf("%s%s%s",current_pwd,PATH_SEPARATOR,"info"));
    }
    char *cur_file_path=NULL;
    cur_file_path = g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"info",PATH_SEPARATOR,info_filename);
    // 在info文件夹下创建一个txt文件
    FILE *file = fopen(cur_file_path, "w");
    if (file == NULL)
    {
        printf("无法创建txt文件\n");
        return 1;
    }
    fclose(file);
    printf("成功创建文件 %s,page = %d\n",cur_file_path,cur_page_index);
    File new_file ={info_filename,cur_file_path,file,cur_page_index};
    g_array_append_val(file_array,new_file);

    //创建文件监听
    if (main_index == 1)
    {
        create_file_monitor(0);
    }

/**-------------------------------------------------create xml------------------------------------------------------------------**/

    // 判断是否存在xml文件夹
    struct stat st2;
    if (stat(g_strdup_printf("%s%s%s",current_pwd,PATH_SEPARATOR,"xml"), &st2) == -1)
    {
        // 如果不存在，则创建info文件夹
        MKDIR_XML;
        printf("成功创建xml文件夹\n"
               "path = %s\n",g_strdup_printf("%s%s%s",current_pwd,PATH_SEPARATOR,"xml"));
    } else
    {
        printf("xml文件夹已存在\n"
               "path = %s\n",g_strdup_printf("%s%s%s",current_pwd,PATH_SEPARATOR,"xml"));
    }

    python_info_path = cur_file_path;
}

int check_properities_file()
{
    gchar* filename= g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"properities.txt") ;

    FILE *file;
    char *content = g_strdup_printf("%s\n%s\n%s\n%s",
                                    "python_script_path=",
                                    "python_system_interpreter=",
                                    "import_api_path=;status=false",
                                    "default_open_file_path="); // 写入文件的内容

    // 尝试以只读方式打开文件
    file = fopen(filename, "r");

    // 如果文件不存在
    if (file == NULL)
    {
        printf("File does not exist. Creating the file...\n");

        // 以写入方式打开文件
        file = fopen(filename, "w");

        // 写入内容到文件
        if (file != NULL)
        {
            fprintf(file, "%s", content);
            printf("Content written to output.txt: %s\n", content);
            fclose(file); // 关闭文件

            // 创建一个消息对话框
            GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                                       GTK_DIALOG_MODAL,
                                                       GTK_MESSAGE_INFO,
                                                       GTK_BUTTONS_OK,
                                                       "配置文件查找失败，请设置配置文件信息");
            // 设置对话框的标题
            gtk_window_set_title(GTK_WINDOW(dialog), "提示");
            // 当用户点击对话框的按钮时，关闭对话框
            g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
            // 显示对话框
            gtk_widget_show_all(dialog);
            return 1;
        } else
        {
            printf("Error opening file for writing.\n");
        }
    } else
    {
        printf("File already exists.\n");
        fclose(file); // 关闭文件
    }
};

/**
 *读取文件配置
 */
int path_init()
{
    //检查 properities 文件是否存在
    int status = check_properities_file();
    if(status == 1)
    {
        printf("properities file creante complete\n");
        return 0;
    }

    const gchar *contents;
    gsize length;
    gchar* filename= g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"properities.txt") ;
    // 读取文件内容
    if (g_file_get_contents(filename, &contents, &length, NULL))
    {
        // 将文件内容插入到文本缓冲区
        debug_textbuff_output("success：读取默认配置文件中 ···\n");

        //对配置文件路径依据不同系统进行初始化
        if(PATH_SEPARATOR == "/")
        {
            printf("linux 操作系统\n");

            char *path = g_strsplit(g_strsplit(contents,"\n",-1)[0],"=",-1)[1];

            printf("开始 python_script_path设置\n");
            python_script_path = g_strdup_printf("%s/%s", g_path_get_dirname(path), g_path_get_basename(path));

            printf("开始 python_system_interpreter\n");
            path = g_strsplit(g_strsplit(contents,"\n",-1)[1],"=",-1)[1];
            python_system_interpreter = path;

            printf("开始 import_api_path\n");
            char **import_path = g_strsplit(g_strsplit(contents,"\n",-1)[2],";",-1);
            path = g_strsplit(import_path[0],"=",-1)[1];
            import_api_path = path;
            path = g_strsplit(import_path[1],"=",-1)[1];
            printf("import_api_path_status = %s\n",path);
            if(strcmp(path,"true") == 0)
            {
                import_api_path_status = TRUE;
                printf("update import status = true\n");
            }else
            {
                import_api_path_status = FALSE;
                printf("update import status = false\n");
            }

            printf("开始 default_open_file_path\n");
            path = g_strsplit(g_strsplit(contents,"\n",-1)[3],"=",-1)[1];
            default_open_file_path = g_strdup_printf("%s",path) ;
            printf("init default_open_file_path = %s\n",default_open_file_path);


        }else
        {
            printf("windows 操作系统\n");

            char *path = g_strsplit(g_strsplit(contents,"\n",-1)[0],"=",-1)[1];

            printf("开始 python_script_path设置\n");
            python_script_path = g_strdup_printf("%s\\%s", g_path_get_dirname(path), g_path_get_basename(path));

            printf("开始 python_system_interpreter\n");
            path =  g_strsplit(g_strsplit(contents,"\n",-1)[1],"=",-1)[1];
            python_system_interpreter = path;

            printf("开始 import_api_path\n");
            char **import_path = g_strsplit(g_strsplit(contents,"\n",-1)[2],";",-1);
            path = g_strsplit(import_path[0],"=",-1)[1];
            import_api_path = path;
            path = g_strsplit(import_path[1],"=",-1)[1];
            printf("import_api_path_status = %s\n",path);

            if(strcmp(path,"true") == 0)
            {
                import_api_path_status = TRUE;
                printf("update import status = true\n");
            }else
            {
                import_api_path_status = FALSE;
                printf("update import status = false\n");
            }

            printf("开始 default_open_file_path\n");
            path = g_strsplit(g_strsplit(contents,"\n",-1)[3],"=",-1)[1];;
            default_open_file_path = path;
            printf("init default_open_file_path = %s\n",default_open_file_path);
        }

        printf("%s\n",contents);
        debug_textbuff_output(contents);
        debug_textbuff_output("\n");

        printf("value设置\n");
        g_value_set_string(&value_py,python_script_path);
        g_value_set_string(&value_py_sys_inter,python_system_interpreter);
        g_value_set_string(&value_import_api_path,import_api_path);
        g_value_set_string(&value_import_api_status,import_api_path_status ? "true":"false");
        printf("value设置完成\n");

        if(default_open_file_path == NULL)
        {
            //设置文件默认地址
            default_open_file_path = g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"xml",PATH_SEPARATOR,"default_xml.txt");
            default_xml_is_vaild = FALSE;
            printf("无法find default txt文件\n");
            printf("default 设置为 %s\n",default_open_file_path);
            FILE *file3 = fopen(default_open_file_path, "w");
            if (file3 == NULL)
            {
                printf("无法创建txt文件\n");
                return 1;
            }
            printf("成功创建文件 %s\n",default_open_file_path);
            properities_file_save();
            fclose(file3);
            python_xml_path = default_open_file_path;

        }else
        {

            FILE *file2 = fopen(default_open_file_path, "r");
            if (file2 == NULL)
            {
                //设置文件默认地址
                default_open_file_path = g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"xml",PATH_SEPARATOR,"default_xml.txt");

                default_xml_is_vaild = FALSE;
                printf("无法find default txt文件\n");
                printf("default %s\n",default_open_file_path);
                FILE *file3 = fopen(default_open_file_path, "w");
                if (file3 == NULL)
                {
                    printf("无法创建txt文件\n");
                    return 1;
                }
                printf("成功创建文件 %s\n",default_open_file_path);
                properities_file_save();
                fclose(file3);
            }else
            {
                default_xml_is_vaild = TRUE;
                fclose(file2);
            }
            python_xml_path = default_open_file_path;
        }

    }else
    {
        debug_textbuff_output("error：读取默认配置文件失败！···\n");
        contents= g_strdup_printf("python_script_path=/home/chen/Download/script.py\n");
    }
}

// 检查文件类型是否为pyd
gboolean is_dest_file(const gchar *filename,const gchar *file_type)
{
    const gchar *extension = g_strrstr(filename, ".");
    if (extension && g_ascii_strcasecmp(extension, file_type) == 0)
    {
        return TRUE;
    }
    return FALSE;
}

// 显示错误对话框
int show_message_dialog(const gchar *message)
{
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(tep_geany->geany_data->main_widgets->window),
                                               GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_OK_CANCEL,
                                               "%s",
                                               message);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    // 判断用户的响应
    if (response == GTK_RESPONSE_OK)
    {
        // 用户点击了“确定”按钮
        // 执行你的逻辑
    } else if (response == GTK_RESPONSE_CANCEL)
    {
        // 用户点击了“取消”按钮
        // 可以选择执行其他操作或者不做任何操作
    }
    gtk_widget_destroy(dialog);
    return response;
}

/**
 * Generate drag sources (To Be Optimized)
 * @return Generated drag source window component
 */
GtkTreeModel* tep_fill_component_source_widget(void)
{
    GtkTreeStore *store = gtk_tree_store_new(NUM_COM_COLS,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_GTYPE);

    GtkTreeIter iter1, iter2, iter3;

    gtk_tree_store_append(store, &iter1, NULL);
    gtk_tree_store_set(store, &iter1, COL_COM_ACTION_NAME, "Default test steps",
                       COL_DESCRIPTION, "", -1);

    gtk_tree_store_append(store, &iter2, &iter1);
    gtk_tree_store_set(store, &iter2, COL_COM_ACTION_NAME, "New-Block",
                       COL_DESCRIPTION, "",
                       COL_COM_TYPE, TEP_TYPE_BASIC_BLOCK, -1);

    gtk_tree_store_append(store, &iter2, &iter1);
    gtk_tree_store_set(store, &iter2, COL_COM_ACTION_NAME, "Data operations",
                       COL_DESCRIPTION, "", -1);

    gtk_tree_store_append(store, &iter3, &iter2);
    gtk_tree_store_set(store, &iter3, COL_COM_ACTION_NAME, "Comment",
                       COL_DESCRIPTION, "Edit comments",
                       COL_COM_TYPE, TEP_TYPE_COMMENT, -1);

    gtk_tree_store_append(store, &iter3, &iter2);
    gtk_tree_store_set(store, &iter3, COL_COM_ACTION_NAME, "Golbal",
                       COL_DESCRIPTION, "Golbal",
                       COL_COM_TYPE, TEP_TYPE_GLOBAL, -1);

    gtk_tree_store_append(store, &iter3, &iter2);
    gtk_tree_store_set(store, &iter3, COL_COM_ACTION_NAME, "Variable",
                       COL_DESCRIPTION, "Variable",
                       COL_COM_TYPE, TEP_TYPE_VARIABLE, -1);

    gtk_tree_store_append(store, &iter3, &iter2);
    gtk_tree_store_set(store, &iter3, COL_COM_ACTION_NAME, "Thread",
                       COL_DESCRIPTION, "create a thread",
                       COL_COM_TYPE, TEP_TYPE_THREAD, -1);

    gtk_tree_store_append(store, &iter3, &iter2);
    gtk_tree_store_set(store, &iter3, COL_COM_ACTION_NAME, "Emit",
                       COL_DESCRIPTION, "Message transmit",
                       COL_COM_TYPE, TEP_TYPE_EMIT, -1);

    gtk_tree_store_append(store, &iter2, &iter1);
    gtk_tree_store_set(store, &iter2, COL_COM_ACTION_NAME, "Flow control",
                       COL_DESCRIPTION, "", -1);

    gtk_tree_store_append(store, &iter3, &iter2);
    gtk_tree_store_set(store, &iter3, COL_COM_ACTION_NAME, "Exit",
                       COL_DESCRIPTION, "End test (package/project)",
                       COL_COM_TYPE, TEP_TYPE_EXIT,-1);

    gtk_tree_store_append(store, &iter3, &iter2);
    gtk_tree_store_set(store, &iter3, COL_COM_ACTION_NAME, "If-Then-Else",
                       COL_DESCRIPTION, "Conditional execution of test steps",
                       COL_COM_TYPE, TEP_TYPE_IF_ELSE, -1);

    gtk_tree_store_append(store, &iter3, &iter2);
    gtk_tree_store_set(store, &iter3, COL_COM_ACTION_NAME, "Loop",
                       COL_DESCRIPTION, "Repeated execution of test steps",
                       COL_COM_TYPE, TEP_TYPE_LOOP, -1);

    gtk_tree_store_append(store, &iter3, &iter2);
    gtk_tree_store_set(store, &iter3, COL_COM_ACTION_NAME, "Continue",
                       COL_DESCRIPTION, "结束单次循环",
                       COL_COM_TYPE, TEP_TYPE_CONTINUE,-1);

    gtk_tree_store_append(store, &iter3, &iter2);
    gtk_tree_store_set(store, &iter3, COL_COM_ACTION_NAME, "Break",
                       COL_DESCRIPTION, "结束全部循环",
                       COL_COM_TYPE, TEP_TYPE_BREAK,-1);

    gtk_tree_store_append(store, &iter3, &iter2);
    gtk_tree_store_set(store, &iter3, COL_COM_ACTION_NAME, "React-On",
                       COL_DESCRIPTION, "Conditional execution according to case",
                       COL_COM_TYPE, TEP_TYPE_REACT_ON,-1);

    gtk_tree_store_append(store, &iter3, &iter2);
    gtk_tree_store_set(store, &iter3, COL_COM_ACTION_NAME, "Wait",
                       COL_DESCRIPTION, "Suspend execution for definite time",
                       COL_COM_TYPE, TEP_TYPE_WAIT,-1);

    gtk_tree_store_append(store, &iter3, &iter2);
    gtk_tree_store_set(store, &iter3, COL_COM_ACTION_NAME, "Try_Catch",
                       COL_DESCRIPTION,"\"try-catch\" is an error-handling mechanism used to capture and handle exceptions or errors that may occur in a program.",
                       COL_COM_TYPE, TEP_TYPE_TRY_CATCH,-1);

    gtk_tree_store_append(store, &iter3, &iter2);
    gtk_tree_store_set(store, &iter3, COL_COM_ACTION_NAME, "Switch",
                       COL_DESCRIPTION, "Execute different code blocks based on different conditional values, used for multi-branch conditional control.",
                       COL_COM_TYPE, TEP_TYPE_SWITCH,-1);

    gtk_tree_store_append(store, &iter3, &iter2);
    gtk_tree_store_set(store, &iter3, COL_COM_ACTION_NAME, "Case",
                       COL_DESCRIPTION, "Conditional execution according to case",
                       COL_COM_TYPE, TEP_TYPE_SWITCH_CASE,-1);

    return GTK_TREE_MODEL(store);
}

/**
 * Generate drag sources (To Be Optimized)
 * @return Generated drag source window component
 */
void tep_execute_widget()
{
    GtkTreeIter iter1, iter2, iter3, iter4;
    GtkTreeStore *store = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(com_source)));

    gtk_tree_store_append(store, &iter1, NULL);
    gtk_tree_store_set(store, &iter1, COL_COM_ACTION_NAME, "Execution",
                       COL_DESCRIPTION, "", -1);

    if (functions != NULL)
    {
        char *library = functions[0].library;
        char *cur_over = "";

        gtk_tree_store_append(store, &iter2, &iter1);
        gtk_tree_store_set(store, &iter2, COL_COM_ACTION_NAME, library,
                           COL_DESCRIPTION, "",-1);

        for(int i = 0;i < num_functions;i++)
        {
            //printf("%s\n%s\n%s\n%s\n%s\n", functions[i].name,functions[i].description,functions[i].args,functions[i].return_type,functions[i].library);

            if( g_ascii_strcasecmp(library, functions[i].library) == 0)
            {
                if (g_strcmp0(functions[i].overload , "") != 0 )
                {
                    if(g_strcmp0(cur_over,functions[i].name) != 0 )
                    {
                        cur_over = functions[i].overload;
                        gtk_tree_store_append(store, &iter3, &iter2);
                        gtk_tree_store_set(store, &iter3,
                                           COL_COM_ACTION_NAME, functions[i].name,
                                           COL_DESCRIPTION, "",
                                           -1);

                        gtk_tree_store_append(store, &iter4, &iter3);
                        gtk_tree_store_set(store, &iter4,
                                           COL_COM_ACTION_NAME, functions[i].name,
                                           COL_DESCRIPTION, g_strdup_printf("%s( %s )",functions[i].description,functions[i].overload),
                                           COL_COM_TYPE, TEP_TYPE_EXECUTE,-1);

                        cur_over = functions[i].name;
                    }else
                    {
                        gtk_tree_store_append(store, &iter4, &iter3);
                        gtk_tree_store_set(store, &iter4,
                                           COL_COM_ACTION_NAME, functions[i].name,
                                           COL_DESCRIPTION, g_strdup_printf("%s( %s )",functions[i].description,functions[i].overload),
                                           COL_COM_TYPE, TEP_TYPE_EXECUTE,-1);
                    }


                }else
                {
                    gtk_tree_store_append(store, &iter3, &iter2);
                    gtk_tree_store_set(store, &iter3,
                                       COL_COM_ACTION_NAME, functions[i].name,
                                       COL_DESCRIPTION, functions[i].description,
                                       COL_COM_TYPE, TEP_TYPE_EXECUTE, -1);
                }
            }
            else
            {
                library = functions[i].library;
                gtk_tree_store_append(store, &iter2, &iter1);
                gtk_tree_store_set(store, &iter2,
                                   COL_COM_ACTION_NAME, functions[i].library,
                                   COL_DESCRIPTION, "", -1);

                if (g_strcmp0(functions[i].overload , "") != 0 )
                {
                    if(g_strcmp0(cur_over,functions[i].name) != 0 )
                    {
                        cur_over = functions[i].overload;
                        gtk_tree_store_append(store, &iter3, &iter2);
                        gtk_tree_store_set(store, &iter3,
                                           COL_COM_ACTION_NAME, functions[i].name,
                                           COL_DESCRIPTION, "",
                                           -1);

                        gtk_tree_store_append(store, &iter4, &iter3);
                        gtk_tree_store_set(store, &iter4,
                                           COL_COM_ACTION_NAME, functions[i].name,
                                           COL_DESCRIPTION, g_strdup_printf("%s( %s )",functions[i].description,functions[i].overload),
                                           COL_COM_TYPE, TEP_TYPE_EXECUTE,-1);

                        cur_over = functions[i].name;
                    }else
                    {
                        gtk_tree_store_append(store, &iter4, &iter3);
                        gtk_tree_store_set(store, &iter4,
                                           COL_COM_ACTION_NAME, functions[i].name,
                                           COL_DESCRIPTION, g_strdup_printf("%s( %s )",functions[i].description,functions[i].overload),
                                           COL_COM_TYPE, TEP_TYPE_EXECUTE,-1);
                    }
                }else
                {
                    gtk_tree_store_append(store, &iter3, &iter2);
                    gtk_tree_store_set(store, &iter3,
                                       COL_COM_ACTION_NAME, functions[i].name,
                                       COL_DESCRIPTION, functions[i].description,
                                       COL_COM_TYPE, TEP_TYPE_EXECUTE, -1);
                }
            }
        }
    }
}

/**
 * Generate drag destination
 * @return Generated drag destination window component
 */
GtkTreeModel* tep_fill_generate_case_widget(void)
{
    GtkTreeStore *store = gtk_tree_store_new(NUM_GEN_COLS,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_POINTER);

    return GTK_TREE_MODEL(store);
}

/**
 * Get row model
 * @param model Source tree model
 * @param iter Tree row's iter where we want to get the model from
 * @return Row model's pointer
 */
gpointer tep_tree_model_get_row_model(GtkTreeModel *model, GtkTreeIter *iter)
{
    GValue row_model = G_VALUE_INIT;
    gtk_tree_model_get_value(model, iter, COL_ROW_MODEL, &row_model);
    return g_value_get_pointer(&row_model);
}

/**
 * Clean a row after the deletion
 * @param model Source tree model
 * @param iter Tree row's iter we deleted
 */
void tep_tree_clean_row(GtkTreeModel *model, GtkTreeIter *iter)
{
    // Clean self
    gpointer row_model = tep_tree_model_get_row_model(model, iter);
    GString *name = TEP_BASIC_COMPONENT(row_model)->name;
    printf("clear par name %s\n",name->str);

    // Clean children
    GtkTreeIter children;
    if(gtk_tree_model_iter_children(model, &children, iter))
    {
        do
        {
            gpointer child_model = tep_tree_model_get_row_model(model, &children);
            GString *name = TEP_BASIC_COMPONENT(child_model)->name;
            printf("clear child name %s\n",name->str);
            g_object_unref(child_model);
        } while(gtk_tree_model_iter_next(model, &children));
    }
    g_object_unref(row_model);

}

/**
 * Tree iters shear
 */
void iters_shear(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW(gen_case);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);

    GtkTreeModel *model;
    GtkTreeIter iter;
    gpointer *row_model;

    model = gtk_tree_view_get_model(tree_view);
    if(gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        shear = TRUE ;
        copy = FALSE;
        my_iter = iter;
        gtk_tree_model_get(model, &iter, COL_ROW_MODEL, &my_row_model, -1);
        GString *name = g_string_new("");
        name = TEP_BASIC_COMPONENT(my_row_model)->action_name_renderer(my_row_model);
        printf("shear iter %s\n",name);
    }else
    {
        printf(" not iter\n");
    }
}

/**
 * html跳转
 */
void goto_html(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW(com_source);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);

    GtkTreeModel *model;
    GtkTreeIter iter;
    gpointer *row_model;

    model = gtk_tree_view_get_model(tree_view);
    if(gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        GType type;
        gtk_tree_model_get(model, &iter, COL_COM_TYPE, &type, -1);
        char* name;

        gtk_tree_model_get(model, &iter, COL_COM_ACTION_NAME, &name, -1);
        printf("function name = %s\n",name);
        if(type == TEP_TYPE_EXECUTE)
        {
            char* sys_call;
            sys_call = g_strdup_printf("%s %s%s%s%s.htm", "hh.exe",current_pwd,PATH_SEPARATOR,"help.chm::/",name);
            printf("%s\n",sys_call);
            GOTO_HTM;
        }

    }else
    {
        printf("not iter\n");
    }
}

/**
 * Tree iters copy
 */
void iters_copy(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW(gen_case);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);

    GtkTreeModel *model;
    GtkTreeIter iter;
    gpointer *row_model;

    model = gtk_tree_view_get_model(tree_view);
    if(gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        copy =TRUE;
        shear = FALSE;
        my_iter = iter;
        gtk_tree_model_get(model, &iter, COL_ROW_MODEL, &my_row_model, -1);
        GString *name = g_string_new("");
        name = TEP_BASIC_COMPONENT(my_row_model)->action_name_renderer(my_row_model);
        printf("copy iter %s\n",name);
    }else
    {
        printf("not iter\n");
    }
}

/**
 * Tree iters paste
 */
void iters_paste(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW(gen_case);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);

    GtkTreeModel *model;
    GtkTreeIter iter;
    gpointer *row_model;

    model = gtk_tree_view_get_model(tree_view);
    if(gtk_tree_selection_get_selected(selection, &model, &iter))
    {
    }else
    {
        printf("paste iter pos not row\n");
    }

//        gtk_tree_model_get(model, &iter, COL_ROW_MODEL, &row_model, -1);
//        GString *name = g_string_new("");
//        name = TEP_BASIC_COMPONENT(row_model)->action_name_renderer(row_model);
//        printf("paste iter pos in row\n name = %s \n",name->str);
    if(shear)
    {
        GtkTreePath *path = NULL;
        path =  gtk_tree_model_get_path(model,&iter);

        if (path != NULL)
        {
            GtkTreeViewDropPosition pos = GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
            GType type = TEP_TYPE_BASIC_BLOCK;
            tep_gen_case_insert_new_row(gen_case,path,pos,type);

            printf("paste success! \n");
            shear = FALSE ;
        }else
        {
            GtkTreeViewDropPosition pos = GTK_TREE_VIEW_DROP_AFTER;
            GType type = TEP_TYPE_BASIC_BLOCK;
            tep_gen_case_insert_new_row(gen_case,path,pos,type);

            printf("paste success! \n");
            shear = FALSE ;
        }

    }
    if(copy)
    {
        copy_insert_new_row(model,my_iter,iter);
        printf("copy success! \n");
        copy = FALSE ;
    }

}

void iters_expand_all()
{
    gtk_tree_view_expand_all(GTK_TREE_VIEW(gen_case));
}

/**
 * Remove a tree row
 * @param menu_item Clicked menu item
 * @param user_data Passed user data
 */
void tep_remove_row(GtkMenuItem *menu_item, gpointer user_data)
{
    GtkTreeView *view = GTK_TREE_VIEW(user_data);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(view);

    GtkTreeModel *model;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model(view);

    if(gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        GtkWidget *dialog = gtk_message_dialog_new(
                GTK_WINDOW(tep_geany->geany_data->main_widgets->window),
                GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                "Are you sure you want to delete this frame?");

        gtk_window_set_title(GTK_WINDOW(dialog), "Confirmation");

        gint response = gtk_dialog_run(GTK_DIALOG(dialog));

        if(response == GTK_RESPONSE_YES)
        {
            tep_tree_clean_row(model, &iter);
            gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
            debug_textbuff_output("删除成功！\n");
        }

        gtk_widget_destroy(dialog);
    }
}

/**
 * 清理页面所有节点
 */
void clear_tree_nodes()
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case));

    // 获取 GtkTreeView 的选择对象
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(gen_case));
    // 取消选择所有行
    gtk_tree_selection_unselect_all(selection);

    gboolean valid = gtk_tree_model_get_iter_first(model,&iter);
    while(valid)
    {
        gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
        valid = gtk_tree_model_get_iter_first(model,&iter);
    }

    debug_textbuff_output("清除成功！\n");
    printf("清除成功！\n");

}

/**
 * Parameter setting dialog
 * @param menu_item Clicked menu item
 * @param user_data Passed user data
 */
void tep_set_parameters(GtkMenuItem *menu_item, gpointer user_data)
{
    g_print("enter public Widget\n");
    GtkTreeView *view = GTK_TREE_VIEW(gen_case);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(view);

    GtkTreeModel *model;
    GtkTreeIter iter;

    model = gtk_tree_view_get_model(view);
    g_print("gtk_tree_view_get_model\n");
    if(gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        g_print("construct Widget\n");

        GtkWidget *dialog = gtk_dialog_new_with_buttons("Parameters setting",
                                                        GTK_WINDOW(tep_geany->geany_data->main_widgets->window),
                                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                                        "Cancel",
                                                        GTK_RESPONSE_CANCEL,
                                                        "Enter",
                                                        GTK_RESPONSE_ACCEPT,
                                                        NULL);

        GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

        GValue row_model = G_VALUE_INIT;
        gtk_tree_model_get_value(model, &iter, COL_ROW_MODEL, &row_model);

        gpointer p = g_value_get_pointer(&row_model);
        if(TEP_BASIC_COMPONENT(p)->get_widget != NULL)
        {
            g_print("prepare enter private Widget\n");
            TEP_BASIC_COMPONENT(p)->get_widget(GTK_CONTAINER(content), p);

            // 创建一个entry
            GtkWidget *entry = gtk_entry_new();

            gint res = gtk_dialog_run(GTK_DIALOG(dialog));

            if(res == GTK_RESPONSE_ACCEPT)
            {
                TEP_BASIC_COMPONENT(p)->on_enter_clicked(p);
            }

            g_value_unset(&row_model);
        }
        //g_print(" destroy private Widget\n");
        gtk_widget_destroy(dialog);
    }
}


/**
 * Parameter setting dialog
 * @param menu_item Clicked menu item
 * @param user_data Passed user data
 */
void tep_drag_set_parameters(GtkTreeIter iter, gpointer user_data)
{
    g_print(" enter darg Widget \n");
    GtkTreeView *view = GTK_TREE_VIEW(gen_case);
    GtkTreeModel *model;
    model = gtk_tree_view_get_model(view);
    g_print(" gtk_tree_view_get_model  \n");

    g_print(" construct Widget \n");

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Parameters setting",
                                                    GTK_WINDOW(tep_geany->geany_data->main_widgets->window),
                                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "Cancel",
                                                    GTK_RESPONSE_CANCEL,
                                                    "Enter",
                                                    GTK_RESPONSE_ACCEPT,
                                                    NULL);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GValue row_model = G_VALUE_INIT;
    gtk_tree_model_get_value(model, &iter, COL_ROW_MODEL, &row_model);

    gpointer p = g_value_get_pointer(&row_model);
    g_print("gpointer get success\n");

    if(TEP_BASIC_COMPONENT(p)->get_widget != NULL)
    {
        g_print("prepare enter private Widget\n");
        TEP_BASIC_COMPONENT(p)->get_widget(GTK_CONTAINER(content), p);

        gint res = gtk_dialog_run(GTK_DIALOG(dialog));

        if(res == GTK_RESPONSE_ACCEPT)
        {
            TEP_BASIC_COMPONENT(p)->on_enter_clicked(p);
        }

        g_value_unset(&row_model);
    }

    //g_print(" destroy private Widget\n");
    gtk_widget_destroy(dialog);
}

/**
 * 获取拖动组建名称
 */
void set_elements_drag_component_name()
{
    //printf("获取drag_component_name = %s \n",drag_component_name);

    for(int i =0;i<num_functions;i++)
    {
        if(strcmp(functions[i].name,drag_component_name)==0)
        {
            set_function((FunctionInfo*)&functions[i]);
            break;
        }else
        {
            if(i == num_functions-1)
            {
                set_function(&functions[i]);
            }
        }
    }
}

/**
 * Insert a basic block for special components
 * @param model Source tree model
 * @param iter Parent row's iter we want to insert into
 * @param nick_name Basic block's nickname
 */
GtkTreeIter tep_insert_basic_block(GtkTreeStore *model, GtkTreeIter *iter, gchar *nick_name)
{
    GtkTreeIter row_iter;
    gpointer row_model = tep_get_row_model_by_type(TEP_TYPE_BASIC_BLOCK);

    tep_basic_block_set_nick_name(row_model, nick_name);

    gtk_tree_store_insert(GTK_TREE_STORE(model), &row_iter, iter, -1);

    gtk_tree_store_set(GTK_TREE_STORE(model), &row_iter, COL_ROW_MODEL, row_model, -1);
    return row_iter;
}


int ergodic_tree_node(GtkWidget *widget,GtkTreeIter iter,GtkTreeIter par)
{
    z++;
    printf("enter ergodic_tree_node  \n");
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case));
    GtkTreeIter children,children_row_iter;
    printf("start gtk_tree_model_iter_children\n");

    if(&par == NULL)
    {
        return 1;
    }
    if(gtk_tree_model_iter_children(model, &children, &par) )
    {
        printf("find child \n");
        do
        {
            gpointer row_model;
            gtk_tree_model_get(model, &children, COL_ROW_MODEL, &row_model, -1);
            GString *name = TEP_BASIC_COMPONENT(row_model)->action_name_renderer(row_model);
            printf("%s\n",name->str);
            gtk_tree_store_insert(GTK_TREE_STORE(model), &children_row_iter, &iter, -1);

            //row_model = tep_get_row_model_by_type(TEP_TYPE_COMMENT);

            gtk_tree_store_set(GTK_TREE_STORE(model), &children_row_iter, COL_ROW_MODEL, row_model, -1);

            ergodic_tree_node(widget,children_row_iter,children);

        } while(gtk_tree_model_iter_next(model, &children));
    }
    z--;
    if(z == 0)
    {
        gtk_tree_store_remove(GTK_TREE_STORE(model), &par);
    }
}

/**
 * Insert a new row after Drag and Drop operation
 * @param widget Destination tree widget
 * @param path Tree path we dropped
 * @param pos Position we dropped
 * @param type Row model's type
 */
int tep_insert_new_row(GtkWidget *widget,
                       GtkTreePath *path,
                       GtkTreeViewDropPosition pos,
                       GType type)
{
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
    GtkTreeIter iter, cur, parent;
    GtkTreeIter *par;

    char *p = gtk_tree_path_to_string(path);
    if(p != NULL)
    {
        gtk_tree_model_get_iter(model, &cur, path); // get current node from the path

        printf("---------------------------start get tree model\n");
        gpointer p1;
        gtk_tree_model_get(model, &cur, COL_ROW_MODEL, &p1, -1);
        printf("---------------------------start get model type\n");
        GType type = G_TYPE_FROM_INSTANCE(p1);
        GString *typename = g_string_new("");
        typename = g_string_new(g_type_name(type));
        printf("---------------------------parent type = %s\n",typename->str);

        for (guint i = 0; i < not_drag_dest->len; i++)
        {
            GType t = g_array_index(not_drag_dest, GType, i);
            if(type == t)
            {
                pos = GTK_TREE_VIEW_DROP_AFTER;
                break;
            }
        }

        if(gtk_tree_model_iter_parent(model, &parent, &cur)) // get parent node
        {
            par = &parent;
            gtk_tree_path_free(path);
        }
        else
        {
            par = NULL;
        }

        switch(pos)
        {
            case GTK_TREE_VIEW_DROP_AFTER:
                gtk_tree_store_insert_after(GTK_TREE_STORE(model), &iter, par, &cur);
                break;
            case GTK_TREE_VIEW_DROP_BEFORE:
                gtk_tree_store_insert_before(GTK_TREE_STORE(model), &iter, par, &cur);
                break;
            case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
            case GTK_TREE_VIEW_DROP_INTO_OR_AFTER: // same handler, insert into last pos
                gtk_tree_store_insert(GTK_TREE_STORE(model), &iter, &cur, -1); // last line
                break;
            default:
                break;
        }
    }
    else
    {
        gtk_tree_store_append(GTK_TREE_STORE(model), &iter, NULL);
    }

    gpointer row_model ;

    if(current_drag_windows == GEN_CASE_DRAG)
    {
        row_model = my_row_model;
        gtk_tree_store_set(GTK_TREE_STORE(model), &iter, COL_ROW_MODEL, row_model, -1);

        ergodic_tree_node(widget,iter,my_iter);

        return 1;
    }
    row_model = tep_get_row_model_by_type(type);
    gtk_tree_store_set(GTK_TREE_STORE(model), &iter, COL_ROW_MODEL, row_model, -1);

    tep_drag_set_parameters(iter,row_model);

    // handle special components
    if(type == TEP_TYPE_IF_ELSE)
    {
        tep_insert_basic_block(GTK_TREE_STORE(model), &iter, "Then");
        tep_insert_basic_block(GTK_TREE_STORE(model), &iter, "Else");
    }
    if(type == TEP_TYPE_TRY_CATCH)
    {
        tep_insert_basic_block(GTK_TREE_STORE(model), &iter, "try");
        tep_insert_basic_block(GTK_TREE_STORE(model), &iter, "catch");
        tep_insert_basic_block(GTK_TREE_STORE(model), &iter, "finally");

    }
    if(type == TEP_TYPE_SWITCH)
    {
        tep_insert_basic_block(GTK_TREE_STORE(model), &iter, "default");
    }
}


gboolean determine_equal_node(GtkTreeIter iter1, GtkTreeIter iter2)
{
    printf("z=%d\n",z);
    z++;
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case));
    gpointer dest_model;
    gtk_tree_model_get(model, &iter2, COL_ROW_MODEL, &dest_model, -1);

    int id2 = TEP_BASIC_COMPONENT(dest_model)->classId;
    if(z == 1)
    {
        int id1 = 0;
        gpointer drag_model;
        gtk_tree_model_get(model, &iter1, COL_ROW_MODEL, &drag_model, -1);
        id1 = TEP_BASIC_COMPONENT(drag_model)->classId;
        if (id1 == id2)
        {
            printf("无法拖动\n");
            z=0;
            return TRUE;
        }
    }
    GtkTreeIter children;
    if(gtk_tree_model_iter_children(model, &children, &iter1) )
    {
        printf("find child\n");
        do {
            int id1 = 0;
            gpointer drag_model;
            gtk_tree_model_get(model, &children, COL_ROW_MODEL, &drag_model, -1);
            id1 = TEP_BASIC_COMPONENT(drag_model)->classId;
            printf("child %d\n",id1);

            if(id1 == id2)
            {
                printf("无法拖动\n");
                z=0;
                return TRUE;
            }
            if(determine_equal_node(children,iter2))
            {
                z=0;
                return TRUE;
            }
        }while(gtk_tree_model_iter_next(model,&children));
    }
    z = 0;
    return FALSE;
}

int tep_gen_case_insert_new_row(GtkWidget *widget,
                            GtkTreePath *path,
                            GtkTreeViewDropPosition pos,
                            GType type)
{
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
    GtkTreeIter iter, cur, parent;
    GtkTreeIter *par;

    char *p = gtk_tree_path_to_string(path);
    if(p != NULL)
    {
        gtk_tree_model_get_iter(model, &cur, path); // get current node from the path

        printf("---------------------------start get tree model\n");
        gpointer p1;
        gtk_tree_model_get(model, &cur, COL_ROW_MODEL, &p1, -1);
        printf("---------------------------start get  model type\n");
        GType type = G_TYPE_FROM_INSTANCE(p1);
        GString *typename = g_string_new("");
        typename = g_string_new(g_type_name(type));
        printf("---------------------------parent type = %s\n",typename->str);
        for (guint i = 0; i < not_drag_dest->len; i++)
        {
            GType t = g_array_index(not_drag_dest, GType, i);
            if(type == t)
            {
                printf("禁止插入类型，不能插入！",typename->str);
                pos = GTK_TREE_VIEW_DROP_AFTER;
                break;
            }
        }

        //not insert node to child node
        if(determine_equal_node(my_iter,cur))
        {
            return 2;
        }

        // get parent node
        if(gtk_tree_model_iter_parent(model, &parent, &cur))
        {
            par = &parent;
            gtk_tree_path_free(path);
        }
        else
        {
            par = NULL;
        }

        switch(pos)
        {
            case GTK_TREE_VIEW_DROP_AFTER:
                gtk_tree_store_insert_after(GTK_TREE_STORE(model), &iter, par, &cur);
                break;
            case GTK_TREE_VIEW_DROP_BEFORE:
                gtk_tree_store_insert_before(GTK_TREE_STORE(model), &iter, par, &cur);
                break;
            case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
            case GTK_TREE_VIEW_DROP_INTO_OR_AFTER: // same handler, insert into last pos
                gtk_tree_store_insert(GTK_TREE_STORE(model), &iter, &cur, -1); // last line
                break;
            default:
                break;
        }
    }
    else
    {
        gtk_tree_store_append(GTK_TREE_STORE(model), &iter, NULL);
    }

    gpointer row_model ;

    row_model = my_row_model;
    gtk_tree_store_set(GTK_TREE_STORE(model), &iter, COL_ROW_MODEL, row_model, -1);

    ergodic_tree_node(widget,iter,my_iter);

    //gtk_tree_view_expand_row(GTK_TREE_VIEW(gen_case),path,TRUE);
    return 1;
}

int copy_insert_new_row(GtkTreeModel *model,GtkTreeIter par,GtkTreeIter dest)
{
    GString *ret_str = g_string_new("");

    gchar* ret ="";
    GtkTreeIter child;
    if(gtk_tree_model_iter_children(model,&child,&par))
    {
        ret = tep_get_component_inner_code_have_note(model,&child,1);
    }

    gpointer row_model = tep_tree_model_get_row_model(model,&par);
    const char* one_code = TEP_BASIC_COMPONENT(row_model)->get_inner_code(row_model,ret,0);
    char** str =  g_strsplit(one_code,"\"",3);
    if(str[1]!=NULL)
    {
        char* select_state = "FALSE";
        if(TEP_BASIC_COMPONENT(row_model)->select)
        {
            select_state = "TRUE";
        }
        one_code = g_strdup_printf("%s\"%s\" state=\"%s\"%s",str[0],str[1],select_state,str[2]);
    }
    g_string_append(ret_str, one_code);

    printf("content:\n%send\n",ret_str->str);

    tep_xml_get_tree("",FALSE,ret_str->str,dest);

}

/**
 * Drag and drop operation handler
 */
void tep_on_drag_data_get(GtkWidget *widget,
                          GdkDragContext *context,
                          GtkSelectionData *selection_data,
                          guint info,guint time,
                          gpointer data)
{
    current_drag_windows = COM_SOURCE_DRAG;

    GtkTreeView *view = GTK_TREE_VIEW(widget);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
    GList *selected_rows, *row;
    GtkTreeModel *model;
    GString *serialized_data;

    serialized_data = g_string_new("");

    selected_rows = gtk_tree_selection_get_selected_rows(selection, &model);

    for(row = selected_rows; row != NULL; row = g_list_next(row))
    {
        GtkTreeIter iter;
        gtk_tree_model_get_iter(model, &iter, row->data);

        if(!gtk_tree_model_iter_has_child(model, &iter))
        {
            GType type;
            gtk_tree_model_get(model, &iter, COL_COM_TYPE, &type, -1);
            gtk_tree_model_get(model, &iter, COL_COM_ACTION_NAME, &drag_component_name, -1);

            char* mess= g_strdup_printf("添加了 %s 组件\n",drag_component_name);
            printf("%s",mess);
            debug_textbuff_output(mess);
            set_elements_drag_component_name();
            serialized_data = g_string_new(g_type_name(type));
            g_free(mess);
        }
    }

    if(serialized_data->len != 0)
    {
        gtk_selection_data_set(selection_data, gdk_atom_intern("DRAG_DATA", FALSE),
                               8, serialized_data->str, serialized_data->len);
    }

    g_string_free(serialized_data, TRUE);

    g_list_free_full(selected_rows, (GDestroyNotify)gtk_tree_path_free);

}

/**
 * Drag and drop operation handler
 * */
void tep_on_drag_data_received(GtkWidget *widget,GdkDragContext *context,gint x, gint y,
                               GtkSelectionData *selection_data,guint info,guint time,gpointer data)
{
    const char *rec_data = (const char *) gtk_selection_data_get_data(selection_data);
    GType type = g_type_from_name(rec_data);
    gint len = gtk_selection_data_get_length(selection_data);

    if (current_drag_windows == COM_SOURCE_DRAG)
    {
        printf("com_source drag data\n");
    }else
    {
        printf("gen_case drag data\n");
    }

    if (len != -1)
    {
        GtkTreePath *path;
        GtkTreeViewDropPosition pos;

        gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y, &path, &pos);

        //gtk_tree_view_expand_to_path(GTK_TREE_VIEW(gen_case), path);

        if(current_drag_windows == COM_SOURCE_DRAG)
        {
            tep_insert_new_row(widget, path, pos, type);
        }else
        {
            tep_gen_case_insert_new_row(widget, path, pos, type);
        }

        gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(tep_geany->geany_data->main_widgets->notebook));
        char *code = NULL;
        code = tep_get_program_code(gtk_tree_view_get_model(GTK_TREE_VIEW(widget)));
        g_array_append_val(code_array, code);
        g_array_remove_index_fast(code_array, page);
        //printf("%s page=%d\n", g_array_index(array,char*,page),page);

    }
}

static void gen_case_drag_data_get(GtkWidget *widget, GdkDragContext *context, GtkSelectionData *selection_data,
                                   guint info, guint time, gpointer user_data)
{
    current_drag_windows = GEN_CASE_DRAG;

    printf("gen case get data start! \n");

    GtkTreeView *view = GTK_TREE_VIEW(widget);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
    GtkTreeModel *model;
    GString *serialized_data;
    serialized_data = g_string_new(g_type_name(TEP_TYPE_EXIT));

    if(serialized_data->len != 0)
    {
        gtk_selection_data_set(selection_data, gdk_atom_intern("DRAG_DATA", FALSE),
                               8, serialized_data->str, serialized_data->len);
    }

    //获取选择的类对象
    GtkTreeIter iter;
    model = gtk_tree_view_get_model(GTK_TEXT_VIEW(gen_case));
    gtk_tree_selection_get_selected(selection, &model, &iter);
    gtk_tree_model_get(model, &iter, COL_ROW_MODEL, &my_row_model, -1);
    my_iter = iter;

//    Row_Model *rowModel ;
//    rowModel->row_model = my_row_model;
//    rowModel->par_iter = 0;
//    my_row_model = rowModel->row_model;

    //移除选择的树节点
    g_string_free(serialized_data, TRUE);

}

/**
 * Bind signal handler for drag and drop operations
 */
void tep_set_drag_and_drop(void)
{
    g_signal_connect(com_source, "drag-data-get", G_CALLBACK(tep_on_drag_data_get), NULL);
    g_signal_connect(gen_case, "drag-data-get", G_CALLBACK(gen_case_drag_data_get), NULL);
    g_signal_connect(gen_case, "drag-data-received", G_CALLBACK(tep_on_drag_data_received), NULL);
}

int class_num=0;

/**
 * Generate objects by different types
 * @param type Object's type
 * @return Pointer of generated object
 */
gpointer tep_get_row_model_by_type(GType type)
{
    gpointer row_model = g_object_new(type, NULL);
//    TEP_BASIC_COMPONENT(row_model)->classId = class_num++;
//    printf("class id  = %d\n",TEP_BASIC_COMPONENT(row_model)->classId );
    return row_model;
}

/**
 * Generate drag source components' window widget
 * @return Generated widget
 */
GtkWidget* tep_create_component_source_widget(void)
{
    GtkWidget* view = gtk_tree_view_new();
    GtkCellRenderer *renderer;
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
                                                -1, "Action / Name",
                                                renderer, "text", COL_COM_ACTION_NAME, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
                                                -1, "Description",
                                                renderer, "text", COL_DESCRIPTION, NULL);

    GtkTreeModel *model = tep_fill_component_source_widget();

    gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);

    gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(view), TRUE);

    GtkTargetEntry targets = {"text/plain", 0, 0};

    gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(view),
                                           GDK_BUTTON1_MASK, &targets,
                                           1, GDK_ACTION_COPY);

    g_object_unref(model);

    GtkWidget *menu = gtk_menu_new();
    GtkWidget *menu_item_goto_html = gtk_menu_item_new_with_label("打开说明文档");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item_goto_html);

    gtk_widget_show_all(menu);

    g_signal_connect(menu_item_goto_html, "activate", G_CALLBACK(goto_html), view);

    gtk_widget_add_events(view, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(view, "button-press-event", G_CALLBACK(tep_on_right_click), menu);
    g_signal_connect(view, "row-activated", G_CALLBACK(tep_set_parameters), GTK_TREE_VIEW(gen_case) );

    return view;
}

int update_class_property(gpointer row_model , RECEIVE_JSON receiveJson)
{
    GString *color = g_string_new("");
    if(g_strcmp0(receiveJson.result,"true")==0)
    {
        g_string_append(color,"light green");
    }else if(g_strcmp0(receiveJson.result,"false")==0)
    {
        g_string_append(color,"red");
    }else
    {
        g_string_append(color,"white");
    }
    TEP_BASIC_COMPONENT(row_model)->set_cell_color(row_model,color);
    TEP_BASIC_COMPONENT(row_model)->message = g_string_new(receiveJson.message);

    printf("success set color %d %s %s %s\n",TEP_BASIC_COMPONENT(row_model)->classId,color->str,receiveJson.result,receiveJson.message);

}


int find_classId_equal_node(GtkTreeIter iter1,RECEIVE_JSON receiveJson)
{
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case));

    GtkTreeIter children;
    if(gtk_tree_model_iter_children(model, &children, &iter1) )
    {
        printf("find child \n");
        do
        {
            int id1 = 0;
            gpointer row_model;
            gtk_tree_model_get(model, &children, COL_ROW_MODEL, &row_model, -1);
            id1 = TEP_BASIC_COMPONENT(row_model)->classId;
            printf("child %d\n",id1);

            if(id1 == receiveJson.id)
            {
                update_class_property(row_model,receiveJson);
                set_message_node(id1);
                break;
            }
            find_classId_equal_node(children,receiveJson);
        }while(gtk_tree_model_iter_next(model,&children));
    }
}

int find_chilren_node(GtkTreeIter iter1,RECEIVE_JSON receiveJson)
{
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case));

    GtkTreeIter children;
    if(gtk_tree_model_iter_children(model, &children, &iter1) )
    {
        printf("find child \n");
        do
        {
            int id1 = 0;
            gpointer row_model;
            gtk_tree_model_get(model, &children, COL_ROW_MODEL, &row_model, -1);
            id1 = TEP_BASIC_COMPONENT(row_model)->classId;
            printf("child %d\n",id1);

            update_class_property(row_model,receiveJson);

            find_chilren_node(children,receiveJson);
        }while(gtk_tree_model_iter_next(model,&children));
    }
}

int block_index = 1;

/**
 * 节点颜色信息处理
 * @param receiveJson
 * @return
 */
int block_type_handel(RECEIVE_JSON receiveJson)
{
    int i = 0;
    int id = receiveJson.id;

    if(get_message_node(id) != 0)
    {
        printf("该节点已收到\n");
        printf(" receive_json_array len = %d\n",receive_json_array->len);
        for (i = receive_json_array->len - 1; i >= 0; i--)
        {
            RECEIVE_JSON mess = g_array_index(receive_json_array, RECEIVE_JSON , i);
            //printf("mess:\n%d,%s,%s\nend\n",mess.id,mess.result,mess.message);
            if (mess.id == id)
            {
                printf("id相等\n");
                if (g_strcmp0(mess.result,receiveJson.result) == 0)
                {
                    printf("result 相等\n");
                    if (g_strcmp0(mess.message,receiveJson.message) == 0)
                    {
                        printf("message 相等\n");
                        printf("该信息已收到\n");
                        return 1;
                    }else
                    {
                        printf("message %s不相等\n",mess.message);
                    }
                }else
                {
                    printf("result %s不相等\n",mess.result);
                }
                break;

            }else
            {
                printf("id不等于%d\n",mess.id);
                continue;
            }
        }
    }else
    {
        printf("该节点未收到\n");
    }

    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case));
    gpointer row_model;
    GtkTreeIter current_iter;
    gboolean valid = gtk_tree_model_get_iter_first(model, &current_iter);

    printf("遍历开始\n");

    while (valid)
    {
        gtk_tree_model_get(model, &current_iter, COL_ROW_MODEL, &row_model, -1);
        GString *name = TEP_BASIC_COMPONENT(row_model)->action_name_renderer(row_model);

        int classId = TEP_BASIC_COMPONENT(row_model)->classId;
        printf("%s %d\n", name->str, classId);

        if (id == classId)
        {
            printf("id %d = classId = %d\n", id, classId);
            printf("start set color \n");
            update_class_property(row_model, receiveJson);
            set_message_node(id);
            break;
        } else
        {
            find_classId_equal_node(current_iter, receiveJson);
            valid = gtk_tree_model_iter_next(model, &current_iter);
        }
    }
    printf("遍历完成 \n");
    if(i == 0)
    {
        printf("add \n");
        // 尝试向数组中添加元素
        g_array_append_val(receive_json_array, receiveJson);
        printf("add end \n");
    }else
    {
        printf("replace \n");
        g_array_index(receive_json_array,RECEIVE_JSON ,i) = receiveJson;
    }
    // 在 message_cell_func 函数中更新单元格信息后，调用 gtk_tree_view_columns_autosize() 刷新视图
    printf("刷新 %d \n",block_index++);
    gtk_tree_view_columns_autosize(GTK_TREE_VIEW(gen_case));
}

char* debug_type_handel(char* debug_mess)
{
    printf("%s\n",debug_mess);
}

int message_allocation(char* color_json)
{
    printf("message_allocation -------- \n");

    removeSpacesAndNewlines(color_json,'\"');

    char **json_content = g_strsplit(color_json,",",-1);

    int id;
    char* result;
    char* message;
    char* receive_type;

//    //json处理键值对
//    cJSON *json = cJSON_Parse(color_json);
//    if (json == NULL) {
//        const char *error_ptr = cJSON_GetErrorPtr();
//        if (error_ptr != NULL) {
//            fprintf(stderr, "Error before: %s\n", error_ptr);
//        }
//        return 1;
//    }
//
//    const cJSON *jid = cJSON_GetObjectItemCaseSensitive(json, "id");
//    if (cJSON_IsNumber(jid)) {
//        printf("[cJSON] id: %d\n", jid->valueint);
//    }
//
//    const cJSON *jresult = cJSON_GetObjectItemCaseSensitive(json, "result");
//    if (cJSON_IsBool(jresult)) {
//        printf("[cJSON] result: %s\n", cJSON_IsTrue(jresult) ? "true" : "false");
//    }
//
//    const cJSON *jmessage = cJSON_GetObjectItemCaseSensitive(json, "message");
//    if (cJSON_IsString(jmessage) && (jmessage->valuestring != NULL)) {
//        printf("[cJSON] message: \"%s\"\n", jmessage->valuestring);
//    }
//
//    const cJSON *jtype = cJSON_GetObjectItemCaseSensitive(json, "type");
//    if (cJSON_IsString(jtype) && (jtype->valuestring != NULL)) {
//        printf("[cJSON] type: \"%s\"\n", jtype->valuestring);
//    }
//
//    cJSON_Delete(json);

    char* sid = strstr(json_content[0],":")+1;
    if(!sid)
    {
        id = 0;
        printf("not find id ,id set default %d\n",id);
    }else
    {
        removeSpacesAndNewlines(sid,' ');

        if(strlen(sid) == 0)
        {
            printf("sid =%s\nsid 为空\n",sid);
            return 1;
        }
        id = atoi(sid);
        printf("id %d\n",id);
    }

    result = strstr(json_content[1], ":") + 1;
    if(!result)
    {
        result = "false";
        printf("not find result,set default result %s\n", result);
    }else
    {
        removeSpacesAndNewlines(result,' ');
        printf("result %s ,len = %d\n", result,strlen(result));
    }

    message = strstr(json_content[2], ":")+2;
    if(!message)
    {
        message = "not find";
        printf("not find message,set default message \"%s\"\n", message);
    } else
    {
        //removeSpacesAndNewlines(message,' ');
        message = unicode_escape_to_utf8(message);
        printf("message %s\n", message);
    }

    receive_type = strstr(json_content[3], ":");
    if(!receive_type)
    {
        receive_type = "invalid";
        printf("not find receive_type,set default receive_type \"%s\"\n", receive_type);
    }else
    {
        receive_type = receive_type + 1;
        removeSpacesAndNewlines(receive_type,' ');
        printf("receive_type %s\n", receive_type);
    }

    RECEIVE_JSON receiveJson = {id, result, message};

    //判断receiveJson类型
    if(g_strcmp0(receive_type,"block") == 0)
    {
        printf("block type\n");
        block_type_handel(receiveJson);
    }else if (g_strcmp0(receive_type,"debug") == 0)
    {
        printf("debug type\n");
    }else
    {
        printf("invalid type\n");
    }
}


/**
 * dubug 断点
 */
void debug_cell_func(GtkTreeViewColumn *column,GtkCellRenderer *cell,
                     GtkTreeModel *model,GtkTreeIter *iter,gpointer data)
{
    gpointer row_model;
    gtk_tree_model_get(model, iter, COL_ROW_MODEL, &row_model, -1);

    if( TEP_BASIC_COMPONENT(row_model)->debug_point)
    {
        g_object_set(cell, "pixbuf", point_pixbuf, NULL);
    }else
    {
        g_object_set(cell, "pixbuf", NULL, NULL);
    }
}


/**
 * 注释按钮
 */
void select_cell_func(GtkTreeViewColumn *column, GtkCellRenderer *renderer,GtkTreeModel *model,
                      GtkTreeIter *iter, gpointer data)
{
    gpointer row_model;
    gtk_tree_model_get(model, iter, COL_ROW_MODEL, &row_model, -1);

    if( TEP_BASIC_COMPONENT(row_model)->select)
    {
        gtk_cell_renderer_toggle_set_active(GTK_CELL_RENDERER_TOGGLE(renderer),TRUE);
    }else
    {
        gtk_cell_renderer_toggle_set_active(GTK_CELL_RENDERER_TOGGLE(renderer),FALSE);
    }
    // 这里可以根据需要设置其他属性
}


/**
 * Action / Name column renderer
 *
 * @param column Not used
 * @param cell Corresponding cell
 * @param model Source tree model
 * @param iter Tree row's iter we selected
 * @param data Not used
 */
void action_name_cell_func(GtkTreeViewColumn *column,GtkCellRenderer *cell,GtkTreeModel *model,
                           GtkTreeIter *iter,gpointer data)
{
    gpointer row_model;
    gtk_tree_model_get(model, iter, COL_ROW_MODEL, &row_model, -1);
    GString *name = TEP_BASIC_COMPONENT(row_model)->action_name_renderer(row_model);
    GString *cell_color = TEP_BASIC_COMPONENT(row_model)->get_cell_color(row_model);

    g_object_set(cell, "text", name->str, NULL);

    //g_object_set(cell, "background", "white", NULL);

    if (g_strcmp0(TEP_BASIC_COMPONENT(row_model)->get_cell_color(row_model)->str,"")==0)
    {
        g_object_set(cell, "background","white", NULL);
    }else
    {
        g_object_set(cell, "background", TEP_BASIC_COMPONENT(row_model)->get_cell_color(row_model)->str, NULL);
    }
}


/**
 * Parameter column renderer
 *
 * @param column Not used
 * @param cell Corresponding cell
 * @param model Source tree model
 * @param iter Tree row's iter we selected
 * @param data Not used
 */
void parameter_cell_func(GtkTreeViewColumn *column,GtkCellRenderer *cell,GtkTreeModel *model,
                         GtkTreeIter *iter,gpointer data)
{
    gpointer row_model;
    gtk_tree_model_get(model, iter, COL_ROW_MODEL, &row_model, -1);
    GString *par = TEP_BASIC_COMPONENT(row_model)->parameter_renderer(row_model);
    g_object_set(cell, "text", par->str, NULL);
}


/**
 * Expectation / Value column renderer
 *
 * @param column Not used
 * @param cell Corresponding cell
 * @param model Source tree model
 * @param iter Tree row's iter we selected
 * @param data Not used
 */
void expectation_value_cell_func(GtkTreeViewColumn *column,GtkCellRenderer *cell,GtkTreeModel *model,
                                 GtkTreeIter *iter,gpointer data)
{
    gpointer row_model;
    gtk_tree_model_get(model, iter, COL_ROW_MODEL, &row_model, -1);
    GString *exp = TEP_BASIC_COMPONENT(row_model)->expectation_value_renderer(row_model);
    g_object_set(cell, "text", exp->str, NULL);
}


/**
 * Comment column renderer
 *
 * @param column Not used
 * @param cell Corresponding cell
 * @param model Source tree model
 * @param iter Tree row's iter we selected
 * @param data Not used
 */
void comment_cell_func(GtkTreeViewColumn *column,GtkCellRenderer *cell,GtkTreeModel *model,
                       GtkTreeIter *iter,gpointer data)
{
    gpointer row_model;
    gtk_tree_model_get(model, iter, COL_ROW_MODEL, &row_model, -1);
    GString *com = TEP_BASIC_COMPONENT(row_model)->comment_renderer(row_model);
    g_object_set(cell, "text", com->str, NULL);
}

/**
 * Message column renderer
 *
 * @param column Not used
 * @param cell Corresponding cell
 * @param model Source tree model
 * @param iter Tree row's iter we selected
 * @param data Not used
 */
void message_cell_func(GtkTreeViewColumn *column,GtkCellRenderer *cell,GtkTreeModel *model,
                       GtkTreeIter *iter,gpointer data)
{
    gpointer row_model;
    gtk_tree_model_get(model, iter, COL_ROW_MODEL, &row_model, -1);
    GString *name = TEP_BASIC_COMPONENT(row_model)->action_name_renderer(row_model);
    GString *cell_color = TEP_BASIC_COMPONENT(row_model)->get_cell_color(row_model);
    g_object_set(cell, "text", TEP_BASIC_COMPONENT(row_model)->message->str, NULL);

}

/**
 * Handle right click menu in generated tree model
 *
 * @param widget Clicked widget
 * @param event Click event
 * @param user_data passed user data
 *
 * @return Success
 */
static gboolean tep_on_right_click(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    if (event->button == GDK_BUTTON_SECONDARY)
    {
        // Right-click
        GtkTreeView *tree_view = GTK_TREE_VIEW(widget);
        GtkWidget *menu = GTK_WIDGET(user_data);

        GtkTreePath *path = NULL;
        GtkTreeViewColumn *column = NULL;
        gint cell_x, cell_y;

        if (gtk_tree_view_get_path_at_pos(tree_view, event->x, event->y, &path, &column, &cell_x, &cell_y))
        {
            // Check if the cursor is inside the allocated area of a row
            GtkTreeIter iter;
            GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
            //gtk_widget_set_sensitive(menu_item_delete, FALSE);
            if (gtk_tree_model_get_iter(model, &iter, path))
            {
                // Show the context menu
                gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
            }

            gtk_tree_path_free(path);
            printf("row\n");
            gen_case_click_is_row = TRUE;
        }else
        {
            printf(" not row\n");
            gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
            // 获取 GtkTreeView 的选择对象
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(gen_case));
            // 取消选择所有行
            gtk_tree_selection_unselect_all(selection);
            gen_case_click_is_row = FALSE;
        }
    }
    return FALSE;
}

static gboolean file_textbuffer_on_right_click(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    if (event->button == GDK_BUTTON_SECONDARY)
    {
        // Right-click
        GtkTextView *tree_view = GTK_TREE_VIEW(widget);
        GtkWidget *menu = GTK_WIDGET(user_data);

        gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
                       NULL, NULL, event->button, event->time);
    }

    return FALSE;
}

static gboolean on_button_press_event(GtkWidget *widget, GdkEventButton *event, GeanyDocument *doc)
{
    if (event->button == GDK_BUTTON_SECONDARY)
    {
        // 如果是右键点击，调用自定义的右键菜单处理函数
        file_textbuffer_on_right_click(widget, event, doc);
        return TRUE; // 阻止默认右键菜单的显示
    }
    return FALSE;
}

//更改子结点颜色
void change_child( GtkTreeModel *model,GtkTreeIter iter,gboolean selected)
{
    printf("checked 遍历开始");
    do
    {
        gpointer row_model;

        gtk_tree_model_get(model, &iter, COL_ROW_MODEL, &row_model, -1);
        printf("id = %d\n", TEP_BASIC_COMPONENT(row_model)->classId);
        TEP_BASIC_COMPONENT(row_model)->select = selected;

        GtkTreeIter child;
        if(gtk_tree_model_iter_children(model,&child,&iter))
        {
            change_child(model,child,selected);
        }
    } while(gtk_tree_model_iter_next(model, &iter));
    printf("checked 遍历完成");
}

int on_checkbox_toggled(GtkCellRendererToggle *renderer, gchar *path, gpointer data)
{
    GtkTreeIter iter,child;
    GtkListStore *list_store = GTK_LIST_STORE(data);

    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case) );
    gpointer row_model;
    if (gtk_tree_model_get_iter_from_string(model, &iter, path))
    {
        gboolean checked;
        gtk_tree_model_get(model, &iter, COL_ROW_MODEL, &row_model, -1);
        printf("id = %d\n", TEP_BASIC_COMPONENT(row_model)->classId);

        if(g_strcmp0(TEP_BASIC_COMPONENT(row_model)->name->str,"block") == 0)
        {
            char* self_name =tep_basic_block_get_nick_name(row_model);
            if (g_strcmp0(self_name,"block") != 0)
            {
                return 1;
            }
        }

        checked = TEP_BASIC_COMPONENT(row_model)->select ;
        TEP_BASIC_COMPONENT(row_model)->select = !checked;

        //checked child遍历
        if(gtk_tree_model_iter_children(model,&child,&iter))
        {
            change_child(model,child,!checked);
            // 在 message_cell_func 函数中更新单元格信息后，调用 gtk_tree_view_columns_autosize() 刷新视图
            gtk_tree_view_columns_autosize(GTK_TREE_VIEW(gen_case));
        }

    }
}


gboolean on_debug_activated(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW(gen_case);

    GtkTreePath *path = NULL;
    GtkTreeViewColumn *column = NULL;
    gint cell_x, cell_y;

    gint start_x = 20;
    gint end_x = 50;

    if (gtk_tree_view_get_path_at_pos(tree_view, event->x, event->y, &path, &column, &cell_x, &cell_y))
    {
        GtkTreeViewColumn *first_column = gtk_tree_view_get_column(GTK_TREE_VIEW(tree_view), 0);

        int dep = gtk_tree_path_get_depth(path);
        printf("click x = %d,dep = %d\n",cell_x,dep);
        int width = 15*dep;
        GtkTreeIter iter;
        GtkTreeModel *model = gtk_tree_view_get_model(tree_view);

        if (gtk_tree_model_get_iter(model, &iter, path))
        {
            if (gtk_tree_model_iter_has_child(model,&iter))
            {
                width = width + 5;
            }
            //判断范围在第一列并且不是展开按钮
            if(first_column ==column && cell_x > width)
            {
                // Check if the cursor is inside the allocated area of a row

                //gtk_widget_set_sensitive(menu_item_delete, FALSE);

                gpointer row_model;
                gtk_tree_model_get(model, &iter, COL_ROW_MODEL, &row_model, -1);

                gboolean flag = TEP_BASIC_COMPONENT(row_model)->debug_point;
                int class_id = TEP_BASIC_COMPONENT(row_model)->classId;
                const char *mess = "";
                //断点类型判断(add/del)
                if (flag)
                {
                    mess = g_strdup_printf("{\"command\":\"break\",\"type\":\"del\",\"id\":%d}", class_id);

                    for (int i = 0; i < debug_point->len; ++i)
                    {
                        int point = g_array_index(debug_point, int, i);
                        if (class_id == point)
                        {
                            g_array_remove_index(debug_point, i);
                            break;
                        }
                    }
                } else
                {
                    mess = g_strdup_printf("{\"command\":\"break\",\"type\":\"add\",\"id\":%d}", class_id);
                    g_array_append_val(debug_point, class_id);
                }
                sendMessageToClient(mess);
                TEP_BASIC_COMPONENT(row_model)->debug_point = !flag;

                // 在 message_cell_func 函数中更新单元格信息后，调用 gtk_tree_view_columns_autosize() 刷新视图
                gtk_tree_view_columns_autosize(GTK_TREE_VIEW(gen_case));
            }
        }
    }
    return FALSE;
}


/**
 * Generate drag destination components' window widget
 *
 * @return Generated widget
 */
GtkWidget* tep_create_generate_case_widget(void)
{
    GtkWidget *view = gtk_tree_view_new();

    // 创建或获取一个GdkPixbuf作为图标
    char* point_pix_path = g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"point.png");

    point_pixbuf = gdk_pixbuf_new_from_file(point_pix_path, NULL);
    point_pixbuf = gdk_pixbuf_scale_simple(point_pixbuf,18, 18, GDK_INTERP_BILINEAR);

    // 创建渲染器
    GtkCellRenderer *renderer;

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(view),
                                               -1, "debug",
                                               renderer, debug_cell_func,
                                               NULL, NULL);


    g_signal_connect(G_OBJECT(view), "button-press-event", G_CALLBACK(on_debug_activated), NULL);

    renderer= gtk_cell_renderer_toggle_new();
    gtk_cell_renderer_toggle_set_active(GTK_CELL_RENDERER_TOGGLE(renderer),TRUE);

    gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(view),
                                               -1, "select",
                                               renderer, select_cell_func,
                                               NULL, NULL);
    g_signal_connect(renderer, "toggled", G_CALLBACK(on_checkbox_toggled), NULL);

    renderer = gtk_cell_renderer_text_new();

    gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(view),
                                               -1, "Action / Name",
                                               renderer, action_name_cell_func,
                                               NULL, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(view),
                                               -1, "Parameter",
                                               renderer, parameter_cell_func,
                                               NULL, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(view),
                                               -1, "Expectation / Value",
                                               renderer, expectation_value_cell_func,
                                               NULL, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(view),
                                               -1, "Comment",
                                               renderer, comment_cell_func,
                                               NULL, NULL);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_data_func(GTK_TREE_VIEW(view),
                                               -1, "MESSAGE",
                                               renderer, message_cell_func,
                                               NULL, NULL);

    //设置视图列宽度
    GList *list = gtk_tree_view_get_columns(GTK_TREE_VIEW(view));
    int index = 0;
    while(list != NULL)
    {
        GtkTreeViewColumn *column = list->data;
        if(index == 0){
            gtk_tree_view_column_set_min_width(column, 30);
        }else if(index == 1)
        {
            gtk_tree_view_column_set_min_width(column, 45);
        }else{
            gtk_tree_view_column_set_min_width(column, 280);

        }
        list = list->next;
        index++;
    }

    GtkTreeModel *model = tep_fill_generate_case_widget();
    geany_plugin_set_data(tep_geany, model, NULL);

    gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);

    gtk_tree_view_expand_all(GTK_TREE_VIEW(view));

    gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(view), TRUE);

    GtkTargetEntry targets = {
            "text/plain", 0, 0
    };

    gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(view), &targets,1, GDK_ACTION_COPY);


    gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(view),
                                           GDK_BUTTON1_MASK, &targets,
                                           1, GDK_ACTION_COPY);
    //gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(view), &targets,1, GDK_ACTION_COPY);

    g_object_unref(model);

    GtkWidget *menu = gtk_menu_new();
    GtkWidget *menu_item_delete = gtk_menu_item_new_with_label("删除");
    GtkWidget *menu_item_clear = gtk_menu_item_new_with_label("清除全部节点");
    GtkWidget *menu_item_expand_all = gtk_menu_item_new_with_label("展开全部节点");
    GtkWidget *menu_item_para = gtk_menu_item_new_with_label("参数设置");
    GtkWidget *menu_item_shear = gtk_menu_item_new_with_label("剪切");
    GtkWidget *menu_item_copy = gtk_menu_item_new_with_label("复制");
    GtkWidget *menu_item_paste = gtk_menu_item_new_with_label("粘贴");

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item_shear);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item_copy);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item_paste);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item_delete);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item_clear);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item_expand_all);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item_para);

    gtk_widget_show_all(menu);

    g_signal_connect(menu_item_shear, "activate", G_CALLBACK(iters_shear), view);
    g_signal_connect(menu_item_copy, "activate", G_CALLBACK(iters_copy), view);
    g_signal_connect(menu_item_paste, "activate", G_CALLBACK(iters_paste), view);
    g_signal_connect(menu_item_expand_all, "activate", G_CALLBACK(iters_expand_all), view);
    g_signal_connect(menu_item_delete, "activate", G_CALLBACK(tep_remove_row), view);
    g_signal_connect(menu_item_clear, "activate", G_CALLBACK(clear_tree_nodes), view);
    g_signal_connect(menu_item_para, "activate", G_CALLBACK(tep_set_parameters), view);

    gtk_widget_add_events(view, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(view, "button-press-event", G_CALLBACK(tep_on_right_click), menu);
    g_signal_connect(view, "row-activated", G_CALLBACK(tep_set_parameters), GTK_TREE_VIEW(gen_case) );

    return view;
}


/**
 * Generate a new scroll window
 *
 * @param child Content we want to insert into this generated scroll window
 *
 * @return Generated scroll window
 */
GtkWidget* tep_get_new_scrolled_window(GtkWidget *child)
{
    GtkWidget *win;
    win = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(win),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    gtk_container_add(GTK_CONTAINER(win), child);

    return win;
}


void on_properities_entrys_changed(GtkEntry *entry, gpointer user_data)
{
    const gchar *text = gtk_entry_get_text(entry);
    g_value_set_string(user_data, text);
    g_print("%s\n",text);
}

// 回调函数，处理复选按钮的点击事件
void properity_on_checkbox_toggled(GtkWidget *widget, gpointer data)
{
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    if (active)
    {
        g_value_set_string(&value_import_api_status, "true");
        g_print("复选按钮被选中了！\n");
    } else
    {
        g_value_set_string(&value_import_api_status, "false");
        g_print("复选按钮被取消选中了。\n");
    }
}


void on_properities_button_clicked(GtkButton *button, GtkEntry *entry)
{
    GtkFileChooserDialog *dialog;
    // 创建文件选择对话框
    dialog = gtk_file_chooser_dialog_new("配置文件选择",
                                         GTK_WINDOW(tep_geany->geany_data->main_widgets->window),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         "Cancel",GTK_RESPONSE_CANCEL,
                                         "Open",GTK_RESPONSE_ACCEPT,
                                         NULL);
    // 设置初始文件夹
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), g_strdup_printf("%s%s%s",cwd,PATH_SEPARATOR,"info"));

    gchar *filename;
    // 运行对话框并获取文件路径
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        gtk_entry_set_text(GTK_ENTRY(entry), filename);
        printf("filename =%s \n",filename);
    }
    // 关闭对话框
    gtk_widget_destroy(GTK_WIDGET(dialog));

}

void on_properities_button_clicked2(GtkButton *button, GtkEntry *entry)
{
    GtkFileChooserDialog *dialog;
    // 创建文件选择对话框
    dialog = gtk_file_chooser_dialog_new("配置文件选择",
                                         GTK_WINDOW(tep_geany->geany_data->main_widgets->window),
                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                         "Cancel",GTK_RESPONSE_CANCEL,
                                         "Open",GTK_RESPONSE_ACCEPT,
                                         NULL);
    // 设置初始文件夹
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), g_strdup_printf("%s%s%s",cwd,PATH_SEPARATOR,"info"));

    gchar *filename;
    // 运行对话框并获取文件路径
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        gtk_entry_set_text(GTK_ENTRY(entry), filename);
        printf("filename =%s \n",filename);
    }
    // 关闭对话框
    gtk_widget_destroy(GTK_WIDGET(dialog));

}
/**
 *设置配置文件内容
 */
int on_properities_button_clicked()
{
    GtkTextBuffer *buffer;
    GtkTextView *textview;
    gchar *filename;
    gchar *contents;
    gsize length;

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Parameters setting",
                                                    GTK_WINDOW(tep_geany->geany_data->main_widgets->window),
                                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    "Cancel",
                                                    GTK_RESPONSE_CANCEL,
                                                    "Enter",
                                                    GTK_RESPONSE_ACCEPT,
                                                    NULL);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(content), box);
    gtk_widget_set_size_request(box, 430, 100);
    /* ---------------------------------------python_script_path------------------------------------------ */

    GtkWidget *python_script_path_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_add(GTK_CONTAINER(box), python_script_path_box);

    GtkWidget *label1 = gtk_label_new("python_script_path: ");
    GtkWidget *entry1 = gtk_entry_new();
    gtk_widget_set_size_request(entry1,330,30);
    gtk_entry_set_text(GTK_ENTRY(entry1), python_script_path);

    g_signal_connect(entry1, "changed", G_CALLBACK(on_properities_entrys_changed),&value_py);
    printf("%s", g_value_get_string(&value_py));
    GtkWidget *button1 = gtk_button_new_with_label("导入");
    g_signal_connect(button1, "clicked", G_CALLBACK(on_properities_button_clicked),GTK_ENTRY(entry1));

    gtk_box_pack_start(GTK_BOX(python_script_path_box), label1, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(python_script_path_box), entry1, FALSE, FALSE, 1);
    gtk_box_pack_end(GTK_BOX(python_script_path_box), button1, FALSE, FALSE, 5);

    /* ---------------------------------------python_output_path------------------------------------------ */

    GtkWidget *python_system_interpreter_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_add(GTK_CONTAINER(box), python_system_interpreter_box);

    GtkWidget *label2 = gtk_label_new("python_system_interpreter：");
    GtkWidget *entry2 = gtk_entry_new();
    gtk_widget_set_size_request(entry2,270,30);
    gtk_entry_set_text(GTK_ENTRY(entry2), python_system_interpreter);

    g_signal_connect(entry2, "changed", G_CALLBACK(on_properities_entrys_changed),&value_py_sys_inter);
    printf("%s", g_value_get_string(&value_py_sys_inter));

    GtkWidget *button2 = gtk_button_new_with_label("导入");
    g_signal_connect(button2, "clicked", G_CALLBACK(on_properities_button_clicked),GTK_ENTRY(entry2));

    gtk_box_pack_start(GTK_BOX(python_system_interpreter_box), label2, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(python_system_interpreter_box), entry2, FALSE, FALSE, 1);
    gtk_box_pack_end(GTK_BOX(python_system_interpreter_box), button2, FALSE, FALSE, 5);


    /* ---------------------------------------import_api_path------------------------------------------ */


    GtkWidget *import_api_path_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_add(GTK_CONTAINER(box), import_api_path_box);

    // 创建复选按钮
    GtkWidget *checkbox = gtk_check_button_new_with_label("");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), import_api_path_status); // 设置为选中状态
    g_signal_connect(G_OBJECT(checkbox), "toggled", G_CALLBACK(properity_on_checkbox_toggled), NULL);

    GtkWidget *label3 = gtk_label_new("import_api_path: ");
    GtkWidget *entry3 = gtk_entry_new();
    gtk_widget_set_size_request(entry3,300,30);
    gtk_entry_set_text(GTK_ENTRY(entry3), import_api_path);

    g_signal_connect(entry3, "changed", G_CALLBACK(on_properities_entrys_changed),&value_import_api_path);
    printf("%s", g_value_get_string(&value_import_api_path));

    GtkWidget *button3 = gtk_button_new_with_label("导入");
    g_signal_connect(button3, "clicked", G_CALLBACK(on_properities_button_clicked2),GTK_ENTRY(entry3));

    gtk_box_pack_start(GTK_BOX(import_api_path_box), checkbox, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(import_api_path_box), label3, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(import_api_path_box), entry3, FALSE, FALSE, 1);
    gtk_box_pack_end(GTK_BOX(import_api_path_box), button3, FALSE, FALSE, 5);

    gtk_widget_show_all(box);

    gint res = gtk_dialog_run(GTK_DIALOG(dialog));

    if(res == GTK_RESPONSE_ACCEPT)
    {
        printf("value_import_api_status = %s\n", g_value_get_string(&value_import_api_status));
        contents= g_strdup_printf("python_script_path=%s\n"
                                  "python_system_interpreter=%s\n"
                                  "import_api_path=%s;"
                                  "status=%s\n"
                                  "default_open_file_path=%s",
                                  g_value_get_string(&value_py),
                                  g_value_get_string(&value_py_sys_inter),
                                  g_value_get_string(&value_import_api_path),
                                  g_value_get_string(&value_import_api_status),
                                  default_open_file_path);
        // 将修改后的内容写回到文件中
        if (g_file_set_contents(g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"properities.txt"), contents, -1, NULL)) {
            debug_textbuff_output("\n配置文件写入完成\n");
            g_print("%s\n",g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"properities.txt"));
            g_print("File saved successfully.\n");
            path_init();

            if((!execute_api_stastus) && import_api_path_status)
            {
                execute_api_stastus = TRUE;
                import_call_python_script(g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"main.py"),
                                          "main",import_api_path);
                tep_execute_widget();
            }

        } else
        {
            debug_textbuff_output("\n配置文件写入失败\n");
            g_print("Failed to save file.\n");
        }
        // 释放内存
        g_free(contents);
    }
    gtk_widget_destroy(dialog);
}

/**
 *配置文件管理
 */
void properities_btn_init(GeanyPlugin *plugin)
{
    properities_btn = gtk_menu_item_new_with_mnemonic("HQ配置设置");
    gtk_widget_show(properities_btn);
    gtk_container_add(GTK_CONTAINER(plugin->geany_data->main_widgets->tools_menu ), properities_btn);
    g_signal_connect(properities_btn, "activate", G_CALLBACK(on_properities_button_clicked),NULL);
}


static gboolean incoming_callback (GSocketService *service,GSocketConnection *connection,
                                   GObject *source_object,gpointer user_data)
{
    printf("事件触发！\n");
    GInputStream *istream;
    gchar buffer[1024];
    gsize bytes_read;

    istream = g_io_stream_get_input_stream (G_IO_STREAM (connection));
    g_input_stream_read_all (istream, buffer, sizeof buffer, &bytes_read, NULL, NULL);
    buffer[bytes_read] = 0;

    g_print ("Received: %s\n", buffer);
    debug_textbuff_output(buffer);
    return FALSE;
}


/**
 * 调用python函数处理脚本
 */
int fill_xml_file(char* params)
{
    FILE *file = fopen(python_xml_path, "w");

    // 检查文件是否成功打开
    if (file == NULL)
    {
        printf("无法打开文件\n");
        return 1;
    }

    // 写入内容到文件
    fprintf(file,params);

    // 关闭文件
    fclose(file);

    printf("xml内容已成功写入到文件\n");
}


int call_python_script(char* script_path,char* functon_name,char* params,char* mode)
{
    fill_xml_file(params);

    char *system_title = g_strdup_printf("%s %s", python_system_interpreter, script_path);


    if (system_title == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate memory for system_title.\n");
        return 1;
    }
    char *system_parms = g_strdup_printf("\"%s\" \"%s\"",
                                         python_info_path,
                                         python_xml_path);
    printf("system_title=%s\n",system_title);
    printf("system_parms=%s\n",system_parms);

    char *system_content = g_strdup_printf("%s %s",system_title,system_parms);

    if(g_strcmp0(mode,"debug") == 0)
    {
        // 使用 GString 来构建字符串
        GString *str = g_string_new("[");
        for (int i = 0; i < debug_point->len; ++i) {
            int value = g_array_index(debug_point, int, i);
            g_string_append_printf(str, "%d", value);
            if (i < debug_point->len - 1) {
                g_string_append(str, ",");
            }
        }
        // 打印最终字符串
        g_string_append(str, "]");
        system_content = g_strdup_printf("%s %s",system_content,str->str);
        g_string_free(str, TRUE);
    }

    printf("input\n%s\n",system_content);

    int system_result = system(system_content);
    if (system_result == -1)
    {
        // 执行失败，输出错误信息
        printf("系统命令执行失败\n");
        return 1;
    } else
    {
        // 执行成功，输出命令的退出状态码
        printf("系统命令执行成功，退出状态码：%d\n", system_result);
    }

    g_free(system_title);
    g_free(system_content);
    g_free(system_parms);
}


/**
 * 调用python脚本
 */
char* import_call_python_script(char* script_path,char* functon_name,char* params)
{
    char *python_interpreter= "/usr/bin/python3";
    char *output_file = g_strdup_printf("%s%s%s",current_pwd,PATH_SEPARATOR,"output.txt");
    char *system_content = g_strdup_printf("%s %s \"%s\" \"%s\"",
                                           python_system_interpreter,
                                           script_path,
                                           params,
                                           output_file);
    printf("system_content = '%s'\n",system_content);
    system(system_content);

    FILE *file;
    char line[512]; // 用于存储每行的内容
    int firstLineAsInt; // 用于存储第一行的整数

    //char *filepath = g_strdup_printf("%s%s%s",params_parent_directory,PATH_SEPARATOR,"output.txt");
    printf("open file path = %s\n",output_file);
    // 打开文件
    file = fopen(output_file, "r");
    if (file == NULL)
    {
        fprintf(stderr, "Error opening file.\n");
    }

    // 读取第一行并转换为整数
    fgets(line, sizeof(line), file);
    firstLineAsInt = atoi(line);
    printf("First line as integer: %d\n", firstLineAsInt);

    int num = 1;
    int list_len = firstLineAsInt;//列表长度40
    gchar **split;
    int split_len=0;
    functions = (FunctionInfo*)malloc(list_len * sizeof(FunctionInfo));
    char *index;

    // 逐行读取文件内容并打印出来
    while (fgets(line, sizeof(line), file) != NULL)
    {
        printf("%d Line: %s  len = %d", num, line, strlen(line));
        if(line[strlen(line)-2] != ']')
        {
            return "error fgets read len";
        }
        char* str_item = line;
        //清除字符串中的 ' \' '&" "
        int x = 0;
        for (int y = 0; y < strlen(str_item); y++)
        {
            if (str_item[y] != '\'' && str_item[y] != ' ')
            {
                str_item[x] = str_item[y];
                x++;
            }
        }
        str_item[x] = '\0'; // 在新字符串的末尾添加 null 终止符
        printf("第%d个函数为： %s\n",num++,str_item);
        split=g_strsplit(str_item,",",-1);

        //装入数据
        int i=num-2;
        functions[i].name = g_utf8_substring(split[0],1, strlen(split[0]));
        functions[i].description = g_utf8_substring(split[1],0, strlen(split[1]));

        char **arg_list = g_strsplit(str_item,"[",-1);
        functions[i].args = g_strsplit(arg_list[2],"]",2)[0];
        functions[i].args_init = g_strsplit(arg_list[3],"]",2)[0];
        printf("init\n%s\n",functions[i].args_init);

        index = g_strrstr(str_item,"],");
        char **cont = g_strsplit(index,",",-1);
        functions[i].return_type = cont[1];
        functions[i].library = cont[2];
        int over_len = strlen(cont[3]);
        if(over_len == 4)
        {
            functions[i].overload = "";
        }else
        {
            functions[i].overload = g_strsplit(cont[3],"]",-1)[0]+1 ;
        }
        printf("overload： %s len  =%d\n",functions[i].overload,over_len);

    }
    num_functions = list_len;

}

// 回调函数，用于更新时间
static gboolean update_time(GtkLabel *label)
{
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M:%S", tm_now);
    gtk_label_set_text(label, buf);
    return TRUE; // 返回 TRUE 以确保定时器继续运行
}

// 弹出窗口的 `delete-event` 事件处理函数
gboolean popup_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    gtk_widget_hide_on_delete(widget);
    return TRUE; // 阻止进一步的 `destroy` 事件处理
}



/**
 * XML_run按钮点击事件的回调函数
 */
int on_XML_run_button_clicked(GtkWidget *button, gpointer data)
{
    //记录刷新次数
    block_index = 1;

    char* mode = (char*)data;
    printf("%s\n",mode);

    gint response ;

    if(g_strcmp0(mode,"run") == 0)
    {
        response = show_message_dialog("是否确定run?");
    }else if (g_strcmp0(mode,"debug") == 0)
    {
        response = show_message_dialog("是否确定debug?");
    }

    if(response == GTK_RESPONSE_CANCEL)
    {
        return GTK_RESPONSE_CANCEL;
    }

    //保存数据
    GtkMenuItem *menuitem = gtk_menu_item_new();
    gpointer user_data;
    tep_save_testcase(menuitem,user_data);

    //初始化 messgae_node
    init_message_node();

    // 清除数组中的所有元素
    g_array_set_size(receive_json_array, 0);
    gtk_text_buffer_set_text(file_textbuffer, "", -1);

//    if (python_scripy_conpleted)
//    {
//        show_message_dialog("run start! ");
//        python_scripy_conpleted = FALSE ;
//    }

    pthread_t thread2;
    int result2 = pthread_create(&thread2, NULL, system_thread, (void*)mode);
    if (result2 != 0)
    {
        printf("Error creating thread2: %d\n", result2);
        return 1;
    }

    if(!debug_windows_state)
    {
        printf("\n\n创建第一个窗口\n\n");
        debug_windows_state = TRUE;

        // 创建一个新窗口
        GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        debug_window = window;

        // 设置窗口为无装饰
        gtk_window_set_decorated(GTK_WINDOW(window), TRUE);

        // 设置窗口类型为浮动
        gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);

        // 设置窗口的默认大小为 300x200 像素
        gtk_window_set_default_size(GTK_WINDOW(window), 120, 120);

        //当点击窗口的关闭按钮时退出 GTK+ 主循环
        g_signal_connect(window, "delete-event", G_CALLBACK(popup_window_delete_event), NULL);

//        // 创建一个标签用于显示键值对
//        debug_label = gtk_label_new("");
//        gtk_container_add(GTK_CONTAINER(window), debug_label);

        // 设置标签文本为键值对
        const char* json_str = "{\"id\": 14, \"result\": true, \"message\": \"no error\", \"type\": \"block\"}";
        create_window_with_json(json_str);

        //char *content = print_json_key_value_pairs(json_str);
        //gtk_label_set_text(GTK_LABEL(debug_label), content);

         //显示窗口及其内容
        gtk_widget_show_all(window);
    }else
    {
        // 设置标签文本为键值对
        const char* json_str = "{\"id\": 15, \"result\": false, \"message\": \"error\", \"type\": \"debug\"}";
        create_window_with_json(json_str);

//        char *content = print_json_key_value_pairs(json_str);
//        gtk_label_set_text(GTK_LABEL(debug_label), content);
//        gtk_widget_show_all(debug_window);
    }

}


/**
 * Debug_run按钮点击事件的回调函数
 */
int on_debug_button_clicked(GtkWidget *button, gpointer data)
{
    printf("on_debug_button_clicked\n");
}


/**
 * Debug_step_run按钮点击事件的回调函数
 */
int on_debug_step_button_clicked(GtkWidget *button, gpointer data)
{
    printf("debug_step\n");
    const char* mess = "{\"command\":\"next\"}";
    sendMessageToClient(mess);
}


/**
 * Debug_step_run按钮点击事件的回调函数
 */
int on_debug_execute_button_clicked(GtkWidget *button, gpointer data)
{
    printf("debug_execute\n");
    sendMessageToClient("{\"command\":\"go\"}");
}


/**
 * import_file按钮点击事件的回调函数
 */
int on_import_file_button_clicked(GtkWidget *button, gpointer data)
{
    // 创建文件选择对话框
    GtkWidget *dialog = gtk_file_chooser_dialog_new("选择要导入的文件",
                                                    GTK_WINDOW(tep_geany->geany_data->main_widgets->window),
                                                    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                    "Cancel",
                                                    GTK_RESPONSE_CANCEL,
                                                    "Open",
                                                    GTK_RESPONSE_ACCEPT,
                                                    NULL);

    // 设置对话框的默认按钮
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), "/home/chen/下载");

    // 运行文件选择对话框
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_ACCEPT)
    {
        // 获取用户选择的文件路径
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        g_print("选择的文件：%s\n", filename);

        import_call_python_script(g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"main.py"),
                                  "main",filename);
        tep_execute_widget();

        // 释放文件路径的内存
        g_free(filename);
    }

    // 关闭对话框
    gtk_widget_destroy(dialog);

}


void export_btn_init(GeanyPlugin *plugin)
{
    export_btn = gtk_menu_item_new_with_mnemonic("HQ另存为");
    gtk_widget_show(export_btn);
    gtk_container_add(GTK_CONTAINER(plugin->geany_data->main_widgets->tools_menu), export_btn);
    g_signal_connect(export_btn, "activate", G_CALLBACK(tep_export_testcase),
                     gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case)));
}


void open_btn_init(GeanyPlugin *plugin)
{
    open_btn = gtk_menu_item_new_with_mnemonic("HQ打开文件");
    gtk_widget_show(open_btn);
    gtk_container_add(GTK_CONTAINER(plugin->geany_data->main_widgets->tools_menu), open_btn);
    g_signal_connect(open_btn, "activate", G_CALLBACK(tep_open_testcase),
                     gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case)));
}


void save_btn_init(GeanyPlugin *plugin)
{
    save_btn = gtk_menu_item_new_with_mnemonic("HQ保存文件");
    gtk_widget_show(save_btn);
    gtk_container_add(GTK_CONTAINER(plugin->geany_data->main_widgets->tools_menu), save_btn);
    g_signal_connect(save_btn, "activate", G_CALLBACK(tep_save_testcase),
                     gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case)));
}

/**
 *运行功能
 */
void run_btn_init(GeanyPlugin *plugin)
{
    //创建工具栏button
    const gchar *imageFileName = "run.png";
    gchar *imageFilePath = g_build_filename("..", "assets", imageFileName, NULL);

    GdkPixbuf *run_pixbuf = gdk_pixbuf_new_from_file(g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"run.png"), NULL);

    run_pixbuf = gdk_pixbuf_scale_simple(run_pixbuf,25, 25, GDK_INTERP_BILINEAR);
    // 创建一个图像小部件并使用Pixbuf设置图标
    GtkWidget *run_icon = gtk_image_new_from_pixbuf(run_pixbuf);
    toolbar_run_btn = gtk_tool_button_new(run_icon, "Run");
    gtk_widget_set_tooltip_text(toolbar_run_btn,"run");
    gtk_widget_show_all(toolbar_run_btn);
    gtk_tool_button_set_use_underline(GTK_TOOL_BUTTON(toolbar_run_btn), TRUE);
    // 将按钮添加到工具栏中
    GtkToolbar *toolbar = GTK_TOOLBAR(plugin->geany_data->main_widgets->toolbar);
    gtk_toolbar_insert(toolbar, GTK_TOOL_ITEM(toolbar_run_btn), -1);
    const char* mode = "run";
    g_signal_connect(toolbar_run_btn, "clicked", G_CALLBACK(on_XML_run_button_clicked),mode);

}

/**
 *debug调试功能
 */
void debug_btn_init(GeanyPlugin *plugin)
{
    //创建工具栏button
    const gchar *imageFileName = "debug.png";
    gchar *imageFilePath = g_build_filename("..", "assets", imageFileName, NULL);

    GdkPixbuf *debug_pixbuf = gdk_pixbuf_new_from_file(g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"debug.png"), NULL);

    debug_pixbuf = gdk_pixbuf_scale_simple(debug_pixbuf,25, 25, GDK_INTERP_BILINEAR);
    // 创建一个图像小部件并使用Pixbuf设置图标
    GtkWidget *debug_icon = gtk_image_new_from_pixbuf(debug_pixbuf);
    toolbar_debug_btn = gtk_tool_button_new(debug_icon, "Debug");
    gtk_widget_set_tooltip_text(toolbar_debug_btn,"debug");
    gtk_widget_show_all(toolbar_debug_btn);
    gtk_tool_button_set_use_underline(GTK_TOOL_BUTTON(toolbar_debug_btn), TRUE);
    // 将按钮添加到工具栏中
    GtkToolbar *toolbar = GTK_TOOLBAR(plugin->geany_data->main_widgets->toolbar);
    gtk_toolbar_insert(toolbar, GTK_TOOL_ITEM(toolbar_debug_btn), -1);
    const char* mode = "debug";
    g_signal_connect(toolbar_debug_btn, "clicked", G_CALLBACK(on_XML_run_button_clicked),mode);
}



/**
 *step through 调试功能
 */
void debug_step_btn_init(GeanyPlugin *plugin)
{
    //创建工具栏button
    const gchar *imageFileName = "debug-step-out.png";
    gchar *imageFilePath = g_build_filename("..", "assets", imageFileName, NULL);

    GdkPixbuf *step_pixbuf = gdk_pixbuf_new_from_file(g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"debug-step-out.png"), NULL);

    step_pixbuf = gdk_pixbuf_scale_simple(step_pixbuf,25, 25, GDK_INTERP_BILINEAR);
    // 创建一个图像小部件并使用Pixbuf设置图标
    GtkWidget *step_icon = gtk_image_new_from_pixbuf(step_pixbuf);
    toolbar_debug_step_btn = gtk_tool_button_new(step_icon, "Step");
    gtk_widget_set_tooltip_text(toolbar_debug_step_btn,"step");
    gtk_widget_show_all(toolbar_debug_step_btn);
    gtk_tool_button_set_use_underline(GTK_TOOL_BUTTON(toolbar_debug_step_btn), TRUE);
    // 将按钮添加到工具栏中
    GtkToolbar *toolbar = GTK_TOOLBAR(plugin->geany_data->main_widgets->toolbar);
    gtk_toolbar_insert(toolbar, GTK_TOOL_ITEM(toolbar_debug_step_btn), -1);
    g_signal_connect(toolbar_debug_step_btn, "clicked", G_CALLBACK(on_debug_step_button_clicked),NULL);
}


/**
 *debug execute 调试功能
 */
void debug_execute_btn_init(GeanyPlugin *plugin)
{
    //创建工具栏button
    const gchar *imageFileName = "execute.png";
    gchar *imageFilePath = g_build_filename("..", "assets", imageFileName, NULL);

    GdkPixbuf *execute_pixbuf = gdk_pixbuf_new_from_file(g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"execute.png"), NULL);

    execute_pixbuf = gdk_pixbuf_scale_simple(execute_pixbuf,25, 25, GDK_INTERP_BILINEAR);
    // 创建一个图像小部件并使用Pixbuf设置图标
    GtkWidget *execute_icon = gtk_image_new_from_pixbuf(execute_pixbuf);
    toolbar_debug_execute_btn = gtk_tool_button_new(execute_icon, "Execute");
    gtk_widget_set_tooltip_text(toolbar_debug_execute_btn,"execute");
    gtk_widget_show_all(toolbar_debug_execute_btn);
    gtk_tool_button_set_use_underline(GTK_TOOL_BUTTON(toolbar_debug_execute_btn), TRUE);
    // 将按钮添加到工具栏中
    GtkToolbar *toolbar = GTK_TOOLBAR(plugin->geany_data->main_widgets->toolbar);
    gtk_toolbar_insert(toolbar, GTK_TOOL_ITEM(toolbar_debug_execute_btn), -1);
    g_signal_connect(toolbar_debug_execute_btn, "clicked", G_CALLBACK(on_debug_execute_button_clicked),NULL);
}


/**
 *导入文件功能
 */
void import_bt_init(GeanyPlugin *plugin)
{
    import_btn = gtk_menu_item_new_with_mnemonic("HQ函数导入");
    gtk_widget_show(import_btn);
    gtk_container_add(GTK_CONTAINER(plugin->geany_data->main_widgets->tools_menu), import_btn);
    g_signal_connect(import_btn, "activate", G_CALLBACK(on_import_file_button_clicked),NULL);
}


gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, GtkWidget *menu)
{
    if (event->button == 3)
    {
        // 右键菜单触发事件
        gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
        return TRUE;
    }
    return FALSE;
}


void clear_text()
{
    // 清除文本缓冲区中的内容
    gtk_text_buffer_set_text(file_textbuffer, "", -1);
}


/**
 *在状态栏中打印信息
 */
void debug_statusbar()
{
    para_set = tep_get_new_scrolled_window(NULL);
    debug_text = gtk_text_view_new();
    debug_textbuffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(debug_text));
    gtk_container_add(GTK_CONTAINER(para_set), debug_text);
    //获取文本缓冲区的起始地址和结束地址
    gtk_text_buffer_get_bounds(debug_textbuffer,&debug_start,&debug_end);
    //插入内容
    gtk_text_buffer_get_end_iter(debug_textbuffer,&debug_end);
//  gtk_text_buffer_insert(debug_textbuffer,&debug_end,"author：华泉信息科技有限公司 \n"
//                                                       "version:1.0\n"
//                                                       "--- --- --- --- --- update date : 2023/9/27 --- --- --- --- \n",-1);

    // 设置GtkTextView为不可编辑状态
    gtk_text_view_set_editable(GTK_TEXT_VIEW(debug_text), FALSE);
    // 隐藏光标
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(debug_text), TRUE);
    // 在插入内容后，将标记放在文本缓冲区的结束位置
    gtk_text_buffer_get_end_iter(debug_textbuffer, &debug_end);
    gtk_text_buffer_create_mark(debug_textbuffer, "end", &debug_end, FALSE);
    // 将窗口滚动到标记位置
    debug_mark = gtk_text_buffer_get_mark(debug_textbuffer, "end");
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(debug_text), debug_mark, 0.0, TRUE, 0.0, 1.0);
    gtk_widget_show_all(para_set);
}


void file_statusbar()
{
    para2_set = tep_get_new_scrolled_window(NULL);
    file_text = gtk_text_view_new();
    file_textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(file_text));
    gtk_container_add(GTK_CONTAINER(para2_set), file_text);
    //获取文本缓冲区的起始地址和结束地址
    gtk_text_buffer_get_bounds(file_textbuffer,&file_start,&file_end);
    //插入内容
    gtk_text_buffer_get_end_iter(file_textbuffer,&file_end);
    gtk_text_buffer_insert(file_textbuffer,&file_end,"状态栏初始化完成!\n", -1);
    // 设置GtkTextView为不可编辑状态
    gtk_text_view_set_editable(GTK_TEXT_VIEW(file_text), FALSE);
    // 隐藏光标
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(file_text), TRUE);
    // 在插入内容后，将标记放在文本缓冲区的结束位置
    gtk_text_buffer_get_end_iter(file_textbuffer, &file_end);
    gtk_text_buffer_create_mark(file_textbuffer, "end", &file_end, FALSE);
    // 将窗口滚动到标记位置
    file_mark = gtk_text_buffer_get_mark(file_textbuffer, "end");
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(file_text), file_mark, 0.0, TRUE, 0.0, 1.0);
    gtk_widget_show_all(para2_set);

    GtkWidget *menu = gtk_menu_new();
    GtkWidget *menu_item_clear = gtk_menu_item_new_with_label("清除");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item_clear);
    gtk_widget_show_all(menu);
    g_signal_connect(menu_item_clear, "activate", G_CALLBACK(clear_text), file_text);
    gtk_widget_add_events(file_text, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(file_text, "button-press-event", G_CALLBACK(file_textbuffer_on_right_click), menu);

}


/**
 * page_clicked
 */
void page_clicked(GtkWidget *widget, gpointer data)
{
    // 获取当前选中的页码
    gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(tep_geany->geany_data->main_widgets->notebook));
    printf("delete page = %d\n",page);
    gtk_notebook_remove_page(GTK_NOTEBOOK(tep_geany->geany_data->main_widgets->notebook), page);
    g_array_remove_index(code_array,page);
    g_array_remove_index(file_array,page);

}


/**
 * 创建一个test case标签页
 */
GtkWidget new_test_case_page()
{
    gen_case=tep_create_generate_case_widget();

    gtk_widget_show_all(gen_case);
    GtkWidget *right_scrolled_window = tep_get_new_scrolled_window( gen_case);
    gtk_widget_show_all(right_scrolled_window);

    //创建标签栏
    GtkWidget *delete_button= new_pixbuf_button("delete", g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"delete.png"),15,15);
    GtkWidget *test_case_label= gtk_label_new("未命名");
    GtkWidget *test_case_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(test_case_box), test_case_label, FALSE, FALSE, 5);
    gtk_box_pack_end(GTK_BOX(test_case_box), delete_button, FALSE, FALSE, 5);
    gtk_widget_show_all(test_case_box);

    int pg=gtk_notebook_append_page(GTK_NOTEBOOK(tep_geany->geany_data->main_widgets->notebook),
                                    right_scrolled_window,
                                    test_case_box);
    g_signal_connect(delete_button, "clicked", G_CALLBACK(page_clicked), NULL);

    printf("add page = %d,pg_adds=%d\n",pg,&pg);
    g_signal_connect(gen_case, "drag-data-received", G_CALLBACK(tep_on_drag_data_received), &pg);
    char *code = "null";
    g_array_append_val(code_array,code);
    create_info_file(pg);
    create_file_monitor(pg);
    //file_monitor_thread_init(pg);
}



/**
 * 增加页面
 */
void toolbar_add_case_btn_init()
{
    toolbar_add_case_btn = new_pixbuf_button("add case", g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"add_case.png"),25,25);
    gtk_toolbar_insert(tep_geany->geany_data->main_widgets->toolbar, GTK_TOOL_ITEM(toolbar_add_case_btn),0);
    g_signal_connect(toolbar_add_case_btn,"clicked",G_CALLBACK(new_test_case_page), NULL);
}


int all_button_init()
{
    //菜单栏
    printf("插入save按钮\n");
    save_btn_init(tep_geany);
    printf("插入open按钮\n");
    open_btn_init(tep_geany);
    printf("插入export按钮\n");
    export_btn_init(tep_geany);
    printf("插入properities按钮\n");
    properities_btn_init(tep_geany);
    printf("import按钮\n");
    import_bt_init(tep_geany);

    //工具栏
    printf("插入run按钮\n");
    run_btn_init(tep_geany);
    printf("插入debug按钮\n");
    debug_btn_init(tep_geany);
    printf("插入step按钮\n");
    debug_step_btn_init(tep_geany);
    printf("插入debug_execute按钮\n");
    debug_execute_btn_init(tep_geany);

    //增加页面按钮
    printf("创建页面增加按钮\n");
    toolbar_add_case_btn_init();

    //设置插件版本信息
    printf("设置插件版本信息\n");
    //tep_geany->info->name = "";
    tep_geany->info->description = "这是一个具有测试功能的插件";
    tep_geany->info->author = "华泉信息科技有限公司";
    tep_geany->info->version = "1.0";

    char* update = "2023/10/09";
    char *mess = g_strdup_printf("author：%s\n"
                                 "version:%s\n"
                                 "--- --- --- --- --- update date: %s--- --- --- --- \n",
                                 tep_geany->info->author,tep_geany->info->version,update);
    gtk_text_buffer_get_start_iter(debug_textbuffer,&debug_start);
    gtk_text_buffer_insert(debug_textbuffer,&debug_start,mess,-1);

}


void variable_init()
{
    code_array = g_array_new(FALSE,TRUE,sizeof(char*));
    thread_array = g_array_new(FALSE,FALSE,sizeof(pthread_t));
    file_array = g_array_new(FALSE,FALSE,sizeof(File));
    not_drag_dest = g_array_new(FALSE,FALSE,sizeof(GType));
    receive_json_array = g_array_new(FALSE,FALSE,sizeof(RECEIVE_JSON));
    receive_json_array_len = 0;
    debug_point = g_array_new(FALSE,FALSE,sizeof(int));

    GType return_type = TEP_TYPE_COMMENT;
    g_array_append_val(not_drag_dest,return_type);
    return_type = TEP_TYPE_VARIABLE;
    g_array_append_val(not_drag_dest,return_type);
    return_type = TEP_TYPE_GLOBAL;
    g_array_append_val(not_drag_dest,return_type);
    return_type = TEP_TYPE_JOIN;
    g_array_append_val(not_drag_dest,return_type);
    return_type = TEP_TYPE_EXIT;
    g_array_append_val(not_drag_dest,return_type);
    return_type = TEP_TYPE_EMIT;
    g_array_append_val(not_drag_dest,return_type);
    return_type = TEP_TYPE_CONTINUE;
    g_array_append_val(not_drag_dest,return_type);
    return_type = TEP_TYPE_BREAK;
    g_array_append_val(not_drag_dest,return_type);
    return_type = TEP_TYPE_WAIT;
    g_array_append_val(not_drag_dest,return_type);
    return_type = TEP_TYPE_EXECUTE;
    g_array_append_val(not_drag_dest,return_type);

    drag_row_models = g_array_new(FALSE,FALSE,sizeof(Row_Model));
    g_value_init(&value_py, G_TYPE_STRING);
    g_value_init(&value_py_sys_inter, G_TYPE_STRING);
    g_value_init(&value_import_api_path, G_TYPE_STRING);
    g_value_init(&value_import_api_status, G_TYPE_STRING);

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("当前工作目录：%s\n", cwd);
        current_pwd = g_strdup_printf("%s%slib%sgeany",cwd,PATH_SEPARATOR,PATH_SEPARATOR);
        printf("current_pwd：%s\n", current_pwd);
    } else
    {
        printf("无法获取当前工作目录\n");
    }


/**-------------------------------------------------create Test plugin------------------------------------------------------------------**/

    current_pwd = g_strdup_printf("%s%s%s",current_pwd,PATH_SEPARATOR,"testcase-plugin");
    // 判断是否存在Test plugin文件夹
    struct stat st2;
    if (stat(current_pwd, &st2) == -1)
    {
        // 如果不存在，则创建info文件夹
        if (PATH_SEPARATOR == "/")
        {
            MKDIR_TEST_PLUGIN
        } else
        {
            MKDIR_TEST_PLUGIN
        }
        printf("成功创建Test_plugin文件夹\n"
               "path = %s\n",current_pwd);
    } else
    {
        printf("xml文件夹已存在\n"
               "path = %s\n",current_pwd);
    }

/**-------------------------------------------------init function_node_name------------------------------------------------------------------**/

    function_node_name = g_array_new(FALSE, FALSE, sizeof(gchar*)); // 指定元素的大小
    char* function_name = "comment";
    g_array_append_val(function_node_name,function_name);
    function_name = "block";
    g_array_append_val(function_node_name,function_name);
    function_name = "global";
    g_array_append_val(function_node_name,function_name);
    function_name = "variable";
    g_array_append_val(function_node_name,function_name);
    function_name = "thread";
    g_array_append_val(function_node_name,function_name);
    function_name = "return";
    g_array_append_val(function_node_name,function_name);
    function_name = "join";
    g_array_append_val(function_node_name,function_name);
    function_name = "emit";
    g_array_append_val(function_node_name,function_name);
    function_name = "exit";
    g_array_append_val(function_node_name,function_name);
    function_name = "if-then-else";
    g_array_append_val(function_node_name,function_name);
    function_name = "loop-for";
    g_array_append_val(function_node_name,function_name);
    function_name = "loop-until";
    g_array_append_val(function_node_name,function_name);
    function_name = "condition";
    g_array_append_val(function_node_name,function_name);
    function_name = "loop-while";
    g_array_append_val(function_node_name,function_name);
    function_name = "break";
    g_array_append_val(function_node_name,function_name);
    function_name = "continue";
    g_array_append_val(function_node_name,function_name);
    function_name = "react-on";
    g_array_append_val(function_node_name,function_name);
    function_name = "wait";
    g_array_append_val(function_node_name,function_name);
    function_name = "try";
    g_array_append_val(function_node_name,function_name);
    function_name = "catch";
    g_array_append_val(function_node_name,function_name);
    function_name = "finally";
    g_array_append_val(function_node_name,function_name);
    function_name = "switch";
    g_array_append_val(function_node_name,function_name);
    function_name = "default";
    g_array_append_val(function_node_name,function_name);
    function_name = "case";
    g_array_append_val(function_node_name,function_name);
    function_name = "execute";
    g_array_append_val(function_node_name,function_name);

    gen_case_click_is_row = FALSE;
    debug_windows_state = FALSE;
}


/**
 * Socket init
 */
int socket_init()
{
    printf("create socket thread!\n");
    socket_mess_lock = FALSE;
    pthread_t thread1;
    CLIENT_SOCKET;
    int result1 = pthread_create(&thread1, NULL, socket_thread, NULL);
    if (result1 != 0)
    {
        printf("Error creating thread1: %d\n", result1);
        return 1;
    }
}



/**
 * import api init
 */
int import_api_init()
{
    printf("import api init\n");

    if(import_api_path_status)
    {
        execute_api_stastus = TRUE;
        import_call_python_script(g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"main.py"),
                                  "main",import_api_path);
        tep_execute_widget();
    }

    printf("open file init\n");
    if(default_xml_is_vaild)
    {
        GtkTreeIter invail_iter;
        tep_xml_get_tree(default_open_file_path,TRUE,"",invail_iter);
    }


}



/**
 * Geany plugin entry: initialization func
 */
gboolean
tep_plugin_init(GeanyPlugin *plugin, gpointer pdata)
{
    variable_init();

    printf("插件准备中---\n");
    tep_geany = plugin;
    printf("创建com_source\n");
    com_source = tep_create_component_source_widget();

    gtk_widget_show_all(com_source);
    printf("创建gen_case\n");
    gen_case = tep_create_generate_case_widget();
    gtk_widget_show_all(gen_case);

    printf("创建print_to_statusbar\n");
    debug_statusbar();
    file_statusbar();

    //拖动功能启动
    tep_set_drag_and_drop();

    printf("创建scrolled\n");
    GtkWidget *left_scrolled_window, *right_scrolled_window, *bottom_scrolled_window, *bottom_scrolled_window2;

    left_scrolled_window = tep_get_new_scrolled_window(com_source);
    gtk_widget_show_all(left_scrolled_window);

    right_scrolled_window = tep_get_new_scrolled_window(gen_case);
    gtk_widget_show_all(right_scrolled_window);

    bottom_scrolled_window = tep_get_new_scrolled_window(para_set);
    gtk_widget_show_all(bottom_scrolled_window);

    bottom_scrolled_window2 = tep_get_new_scrolled_window(para2_set);
    gtk_widget_show_all(bottom_scrolled_window2);

    printf("创建Test steps页面\n");
    com_source_index = gtk_notebook_insert_page(
            GTK_NOTEBOOK(plugin->geany_data->main_widgets->sidebar_notebook),
            left_scrolled_window,
            gtk_label_new("Test steps"),
            0);

    gtk_notebook_set_group_name(GTK_NOTEBOOK(plugin->geany_data->main_widgets->sidebar_notebook),
                                "testcase_plugin");

    gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(plugin->geany_data->main_widgets->sidebar_notebook),
                                    left_scrolled_window, TRUE);

    printf("创建页面删除按钮\n");
    // 设置button图标
    GtkWidget *delete_button = new_pixbuf_button("delete",g_strdup_printf("%s%s%s%s%s",current_pwd,PATH_SEPARATOR,"assets",PATH_SEPARATOR,"delete.png"),15,15);
    GtkWidget *test_case_label = gtk_label_new("Test case");
    GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_VERTICAL,1);
    GtkWidget *test_case_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_add(GTK_CONTAINER(row_box),test_case_box);

    gtk_box_pack_start(GTK_BOX(test_case_box), test_case_label, FALSE, FALSE, 5);
    gtk_box_pack_end(GTK_BOX(test_case_box), delete_button, FALSE, FALSE, 5);
    gtk_widget_show_all(test_case_box);
    g_signal_connect(delete_button, "clicked", G_CALLBACK(page_clicked),NULL);

    printf("创建Test case页面\n");
    gtk_notebook_insert_page(
            GTK_NOTEBOOK(plugin->geany_data->main_widgets->notebook),
            right_scrolled_window,
            row_box,
            0);
    create_info_file(0);

    printf("设置testcase_plugin组\n");
    gtk_notebook_set_group_name(GTK_NOTEBOOK(plugin->geany_data->main_widgets->notebook),
                                "testcase_plugin");

    gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(plugin->geany_data->main_widgets->notebook),
                                    right_scrolled_window, TRUE);

    printf("插入Debug页面\n");
    para_set_index = gtk_notebook_insert_page(
            GTK_NOTEBOOK(plugin->geany_data->main_widgets->message_window_notebook),
            bottom_scrolled_window,
            gtk_label_new("Debug info"),
            0);

    printf("插入File页面\n");
    para2_set_index = gtk_notebook_insert_page(
            GTK_NOTEBOOK(plugin->geany_data->main_widgets->message_window_notebook),
            bottom_scrolled_window2,
            gtk_label_new("File info"),
            1);

    gtk_notebook_set_group_name(GTK_NOTEBOOK(plugin->geany_data->main_widgets->message_window_notebook),
                                "testcase_plugin");

    //保存脚本输出信息
    printf("路径初始化\n");
    path_init();
    printf("按钮初始化\n");
    all_button_init();
    printf("socket初始化\n");
    socket_init();
    printf("api初始化\n");
    import_api_init();
    printf("所有初始化完成\n");

}



/**
 * Geany plugin entry: clean up func
 * */
void tep_plugin_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
    g_print("Cleaning up...\n");

    if(socket_client_id != 0)
    {
        close(socket_server_id);
        close(socket_client_id);
    }

    // 清理monitor资源
    g_object_unref(monitor);
    g_object_unref(monitor_file);

    GtkTreeModel *model = (GtkTreeModel *) pdata;
    GtkTreeIter iter;
    if(gtk_tree_model_get_iter_first(model, &iter))
    {
        tep_tree_cleanup(model, &iter);
    }

    g_array_free(code_array,TRUE);
    g_array_free(thread_array,TRUE);
    g_array_free(file_array,TRUE);
    g_array_free(not_drag_dest,TRUE);
    g_array_free(receive_json_array,TRUE);
    g_array_free(function_node_name,TRUE);
    g_array_free(debug_point,TRUE);

    gtk_widget_destroy(com_source);
    gtk_widget_destroy(gen_case);
    gtk_widget_destroy(para_set);
    gtk_widget_destroy(para2_set);

    gtk_widget_destroy(export_btn);
    gtk_widget_destroy(run_btn);
    gtk_widget_destroy(toolbar_run_btn);
    gtk_widget_destroy(toolbar_add_case_btn);
    gtk_widget_destroy(import_btn);
    gtk_widget_destroy(properities_btn);

    gtk_notebook_remove_page(GTK_NOTEBOOK(tep_geany->geany_data->main_widgets->sidebar_notebook),
                             com_source_index);
    gtk_notebook_remove_page(GTK_NOTEBOOK(tep_geany->geany_data->main_widgets->notebook),
                             gen_case_index);
    gtk_notebook_remove_page(GTK_NOTEBOOK(tep_geany->geany_data->main_widgets->message_window_notebook),
                             para_set_index);
    gtk_notebook_remove_page(GTK_NOTEBOOK(tep_geany->geany_data->main_widgets->message_window_notebook),
                             para2_set_index);

}



/**
 * Clean up the whole generated tree
 *
 * @param model Target tree model
 * @param iter Tree row's iter of the header of tree
 */
void tep_tree_cleanup(GtkTreeModel *model, GtkTreeIter *iter)
{
    g_print("%s", tep_get_program_code(model));
    do
    {
        tep_tree_clean_row(model, iter);
    } while(gtk_tree_model_iter_next(model, iter));
}



/**
 * Get inner codes recursively
 *
 * @param model Generated tree model
 * @param iter Current iter
 * @param tab_count Current tab count
 *
 * @return Inner code for current iter
 */
gchar* tep_get_component_inner_code(GtkTreeModel *model, GtkTreeIter *iter, int tab_count)
{
    if(model == NULL || iter == NULL) return "";

    GString *ret_str = g_string_new("");
    do
    {
        GtkTreeIter child;
        gchar *child_code = "";
        if(gtk_tree_model_iter_children(model, &child, iter))
        {
            child_code = tep_get_component_inner_code(model, &child, tab_count + 1);
        }

        gpointer row_model = tep_tree_model_get_row_model(model, iter);
        if(TEP_BASIC_COMPONENT(row_model)->select)
        {
            g_string_append(ret_str, TEP_BASIC_COMPONENT(row_model)->get_inner_code(row_model,
                                                                                    child_code,
                                                                                    tab_count));
        }

    } while(gtk_tree_model_iter_next(model, iter));

    gchar *ret = g_strdup_printf("%s", ret_str->str);
    g_string_free(ret_str, TRUE);

    return ret;
}



/**
 * Get inner codes recursively
 *
 * @param model Generated tree model
 * @param iter Current iter
 * @param tab_count Current tab count
 *
 * @return Inner code for current iter
 */
gchar* tep_get_component_inner_code_have_note(GtkTreeModel *model, GtkTreeIter *iter, int tab_count)
{
    if(model == NULL || iter == NULL) return "";

    GString *ret_str = g_string_new("");
    do
    {
        GtkTreeIter child;
        gchar *child_code = "";
        if(gtk_tree_model_iter_children(model, &child, iter))
        {
            child_code = tep_get_component_inner_code_have_note(model, &child, tab_count + 1);
        }

        gpointer row_model = tep_tree_model_get_row_model(model, iter);
        const char* one_code = TEP_BASIC_COMPONENT(row_model)->get_inner_code(row_model,
                                                                              child_code,
                                                                              tab_count);
        char** str =  g_strsplit(one_code,"\"",3);
        if(str[1]!=NULL)
        {
            char* select_state = "FALSE";
            if(TEP_BASIC_COMPONENT(row_model)->select)
            {
                select_state = "TRUE";
            }
            one_code = g_strdup_printf("%s\"%s\" state=\"%s\"%s",str[0],str[1],select_state,str[2]);
        }
        g_string_append(ret_str, one_code);
        g_free(str);
    } while(gtk_tree_model_iter_next(model, iter));

    gchar *ret = g_strdup_printf("%s", ret_str->str);
    g_string_free(ret_str, TRUE);

    return ret;
}

/**
 * Synthesize the inner code to export XML code
 *
 * @param model Generated tree model
 *
 * @return Generated full inner code
 */
gchar* tep_get_program_code(GtkTreeModel *model)
{
    gchar *cur_code = "";
    GtkTreeIter iter;
    if(gtk_tree_model_get_iter_first(model, &iter))
    {
        cur_code = tep_get_component_inner_code(model, &iter, 0);
    }
    return cur_code;
}


/**
 * Synthesize the inner code to export XML code
 *
 * @param model Generated tree model
 *
 * @return Generated full inner code
 */
gchar* tep_get_program_code_have_state(GtkTreeModel *model)
{
    gchar *cur_code = "";
    GtkTreeIter iter;
    if(gtk_tree_model_get_iter_first(model, &iter))
    {
        cur_code = tep_get_component_inner_code_have_note(model, &iter, 0);
    }
    return cur_code;
}



// 递归保存 TreeStore 数据到文件
gchar* save_tree_store(GtkTreeModel* model,GtkTreeIter* iter , int dep)
{
    gchar* ret = "";
    // 递归保存子节点
    do
    {
        printf("save_tree_store %d\n", dep);
        ret =g_strdup_printf("%s%d\n", ret,dep);
        GtkTreeIter child_iter;

        if (gtk_tree_model_iter_children(model,&child_iter,iter))
        {
            gchar* child = "";
            child = save_tree_store(model,&child_iter,dep + 1);
            ret = g_strdup_printf("%s%s", ret,child);
        }
    }while(gtk_tree_model_iter_next(model, iter));
    return ret;
}


static void start_element_handler(GMarkupParseContext *context, const gchar *element_name,
                                  const gchar **attribute_names, const gchar **attribute_values,
                                  gpointer user_data, GError **error)
{
    for (int i = 0; attribute_names[i] != NULL; ++i)
    {
        printf("attribute_names %s %s\n",attribute_names[i],attribute_values[i]);
        if(g_strcmp0(attribute_names[i],"state") == 0)
        {
            gboolean checked;
            if (g_strcmp0(attribute_values[i],"TRUE") == 0)
            {
                checked = TRUE;
            }else
            {
                checked = FALSE;
            }
            g_array_append_val(state,checked);
        }
    }

    printf("%s %s\n","start_element_handler",element_name);
    int dep = *(int *)user_data;
    *(int *)user_data = dep + 1 ;
    gboolean isnode = FALSE;
    for(int i = 0;i < function_node_name->len;i++)
    {
        gchar* node = g_array_index(function_node_name, gchar*,i);
        if(g_strcmp0(node,element_name)==0)
        {
//          printf("node %s\n",element_name);
            isnode = TRUE;
            break;
        }
    }

    if(isnode)
    {
        printf("is node\n");
        XML_NODE node;
        GArray *content_array = g_array_new(FALSE,FALSE,sizeof(char*));
        char* name = g_strdup_printf("%s",element_name);
        node.name = name;
        node.deep = dep;
        node.is_row = TRUE;
        node.content = g_string_new("");
        node.contents_len = 0;
        node.contents = content_array;
        node.is_parms = FALSE;
        g_array_append_val(xml_nodes,node);
        printf("------------------------- %s \n",node.name);
    }else
    {
        printf("not node\n");
        XML_NODE node = g_array_index(xml_nodes,XML_NODE,xml_nodes->len-1);
        node.is_parms = TRUE;
        node.is_row = TRUE;
        node.contents_len = 0;
        g_array_index(xml_nodes,XML_NODE,xml_nodes->len-1) = node;
    }

}

static void text_handler(GMarkupParseContext *context, const gchar *text, gsize text_len,
                         gpointer user_data, GError **error)
{
    printf("%s %s (len = %d)\n","text_handler",text, text_len);
    XML_NODE node = g_array_index(xml_nodes,XML_NODE,xml_nodes->len-1);
    if(node.is_parms)
    {
        XML_NODE node = g_array_index(xml_nodes,XML_NODE,xml_nodes->len-1);
        if (node.is_row)
        {
            if (text_len == 0)
            {
                text = "null";
            }
            char* property = g_strdup_printf("%s",text);
            g_array_append_val(node.contents, property);
            printf("%s  len = %d加入内容到 array : %s\n",node.name,node.contents->len-1,property);
            node.contents_len = node.contents_len + 1;
            node.is_row = FALSE;
            g_array_index(xml_nodes,XML_NODE,xml_nodes->len-1) = node;
        }

    }else
    {
        if (node.is_row)
        {
            GString *con = g_string_new("");
            g_string_printf(con,"%s",text);
            XML_NODE node = g_array_index(xml_nodes,XML_NODE,xml_nodes->len-1);
            node.content = con;
            printf("加入内容到content : %s\n",node.content->str);
            node.is_row = FALSE;
            g_array_index(xml_nodes,XML_NODE,xml_nodes->len-1) = node;
        }
    }
}


static void end_element_handler(GMarkupParseContext *context, const gchar *element_name,
                                gpointer user_data, GError **error)
{
    *(int *)user_data = *(int *)user_data - 1 ;
    printf("%s %s\n","end_element_handler",element_name);

}

/**
 * Save XML file
 */
void tep_save_testcase(GtkMenuItem *menuitem, gpointer user_data)
{
    FILE *file = fopen(default_open_file_path, "w");
    if (file != NULL)
    {
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case));
        gchar *code =tep_get_program_code_have_state(gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case)));
        printf("content:\n%s\nend\n",code);
        fprintf(file,code);
        fclose(file);
        g_print("file save success!\n");
        debug_textbuff_output("file save success!\n");
    } else
    {
        g_print("Error opening the file for writing.\n");
    }
}


/**
 * Export XML file
 */
void tep_export_testcase(GtkMenuItem *menuitem, gpointer user_data)
{
    GtkWidget *dialog;
    GtkFileChooser *chooser;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;

    dialog = gtk_file_chooser_dialog_new ("Save Testcase",
                                          GTK_WINDOW(tep_geany->geany_data->main_widgets->window),
                                          action,
                                          _("_Cancel"),
                                          GTK_RESPONSE_CANCEL,
                                          _("_Save"),
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);

    chooser = GTK_FILE_CHOOSER(dialog);

    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), g_strdup_printf("%s%s%s%s",current_pwd,PATH_SEPARATOR,"xml",PATH_SEPARATOR));

    gtk_file_chooser_set_current_name(chooser, "GeanyTestCase.txt");

    gint res = gtk_dialog_run(GTK_DIALOG(dialog));

    if(res == GTK_RESPONSE_ACCEPT)
    {
        gchar *filename = gtk_file_chooser_get_filename(chooser);

        gchar *selected_path = gtk_file_chooser_get_filename(chooser);

        // Open the file for writing
        FILE *file = fopen(selected_path, "w");
        if (file != NULL)
        {
            GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case));

            //生存xml
            gchar *code = tep_get_program_code(gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case)));

            fprintf(file,code);

            //保存路径
            default_open_file_path = selected_path;
            properities_file_save();

            fclose(file);
        } else
        {
            g_print("Error opening the file for writing.\n");
        }
        g_free(filename);
    }

    gtk_widget_destroy(dialog);

}


/**
 * 解析xml name 的 类型
 */
gpointer function_name_get_gpointer(XML_NODE xmlNode,GtkTreeModel *model,int index)
{
    char* name = xmlNode.name;
    printf("function_name_get_gpointer name %s,len = %d\n",name, strlen(name));
    GType type;
    gpointer row_model;
    gboolean have_param = TRUE;
    if(g_strcmp0(name,"block") == 0)
    {
        type = TEP_TYPE_BASIC_BLOCK;
        row_model = tep_get_row_model_by_type(type);
        return row_model;
    } else if (g_strcmp0(name,"comment") == 0)
    {
        type = TEP_TYPE_COMMENT;
    }else if (g_strcmp0(name,"continue") == 0)
    {
        type = TEP_TYPE_CONTINUE;
        row_model = tep_get_row_model_by_type(type);
        return row_model;
    }else if (g_strcmp0(name,"emit") == 0)
    {
        type = TEP_TYPE_EMIT;
    }else if (g_strcmp0(name,"exit") == 0)
    {
        type = TEP_TYPE_EXIT;
        row_model = tep_get_row_model_by_type(type);
        return row_model;
    }else if (g_strcmp0(name,"global") == 0)
    {
        type = TEP_TYPE_GLOBAL;
    }else if (g_strcmp0(name,"variable") == 0)
    {
        type = TEP_TYPE_VARIABLE;
    }
    else if (g_strcmp0(name,"if-then-else") == 0)
    {
        type = TEP_TYPE_IF_ELSE;
        row_model = tep_get_row_model_by_type(type);
        GObjectClass *object_class = G_OBJECT_GET_CLASS(row_model);

        XML_NODE condition = g_array_index(xml_nodes,XML_NODE,index);
        printf(" condition name  %s\n",condition.name);
        if(!condition.is_parms)
        {
            GValue value = G_VALUE_INIT;
            //初始化参数condition
            if (g_strcmp0(condition.name, "condition") == 0)
            {
                g_value_init(&value, G_TYPE_STRING);

                const gchar* con =  condition.content->str;
                g_value_set_string(&value,con);

                object_class->set_property(row_model, 0, &value, NULL);
                printf("set if condition %s\n",con);

                g_array_remove_index(xml_nodes, index);
                printf("if remove condition\n");
                g_value_unset(&value);
            }
        }
        return row_model;
    }else if (g_strcmp0(name,"thread") == 0)
    {
        type = TEP_TYPE_THREAD;
        for (int i = index; i < xml_nodes->len; ++i)
        {
            XML_NODE node = g_array_index(xml_nodes,XML_NODE,i);
            if(g_strcmp0(node.name,"return") == 0)
            {
                row_model = tep_get_row_model_by_type(type);
                GObjectClass *object_class = G_OBJECT_GET_CLASS(row_model);

                GValue value = G_VALUE_INIT;
                g_value_init(&value, G_TYPE_STRING);

                g_value_set_string(&value, node.content->str);
                object_class->set_property(row_model, 1, &value, NULL);
                printf("set thread return = %s\n", g_value_get_string(&value));

                g_value_unset(&value);

                g_array_remove_index(xml_nodes,i);
                printf("remove return\n");
                return row_model;
            }

        }
    }
    else if (g_strcmp0(name,"join") == 0)
    {
        type = TEP_TYPE_JOIN;

    }else if (g_strcmp0(name,"loop-for") == 0)
    {
        printf("set loop-for\n");

        type = TEP_TYPE_LOOP;
        row_model = tep_get_row_model_by_type(type);
        GObjectClass *object_class = G_OBJECT_GET_CLASS(row_model);

        if(xmlNode.is_parms)
        {
            if(xmlNode.contents->len == 3)
            {
                GValue value = G_VALUE_INIT;

                //初始化参数count
                g_value_init(&value, G_TYPE_UINT);
                int i = 0;
                gchar *con = g_array_index(xmlNode.contents, char*, i);
                guint number = atoi(con);
                printf("count %d\n",number);
                g_value_set_uint(&value, number);
                object_class->set_property(row_model, 0, &value, NULL);
                printf("param %d : %s\n", i, con);
                g_value_unset(&value);

                //初始化参数start
                g_value_init(&value, G_TYPE_UINT);
                i = 1;
                con = g_array_index(xmlNode.contents, char*, i);
                number = atoi(con);
                printf("start %d\n",number);
                g_value_set_uint(&value, number);
                object_class->set_property(row_model, 1, &value, NULL);
                printf("param %d : %s\n", i, con);
                g_value_unset(&value);

                //初始化参数step
                g_value_init(&value, G_TYPE_UINT);
                i = 2;
                con = g_array_index(xmlNode.contents, char*, i);
                number = atoi(con);
                printf("step %d\n",number);
                g_value_set_uint(&value, number);
                object_class->set_property(row_model, 2, &value, NULL);
                printf("param %d : %s\n", i, con);
                g_value_unset(&value);

                //初始化参数model
                g_value_init(&value, G_TYPE_UINT);
                number = 2;
                printf("model %d\n",number);
                g_value_set_uint(&value, number);
                object_class->set_property(row_model, 4, &value, NULL);
                printf("param %d : %s\n", i, con);
                g_value_unset(&value);
            }
        }else
        {
            printf("loop-while fail!\n");
        }
        printf("content fill completed\n");
        return row_model;
    }else if (g_strcmp0(name,"loop-while") == 0)
    {
        printf("set loop-while\n");

        type = TEP_TYPE_LOOP;
        row_model = tep_get_row_model_by_type(type);
        GObjectClass *object_class = G_OBJECT_GET_CLASS(row_model);

        XML_NODE condition = g_array_index(xml_nodes,XML_NODE,index);
        printf(" con %s\n",condition.name);
        if(condition.is_parms)
        {
            if(condition.contents->len == 3) {
                GValue value = G_VALUE_INIT;

                //初始化参数condition
                if(g_strcmp0(condition.name ,"condition") == 0)
                {
                    g_value_init(&value, G_TYPE_STRING);

                    g_value_set_string(&value, condition.content->str);
                    object_class->set_property(row_model, 3, &value, NULL);
                    printf("set loop while condition %d\n", g_value_get_string( &value));

                    g_array_remove_index(xml_nodes,index);
                    printf("loop while remove condition\n");
                    g_value_unset(&value);
                }

                //初始化参数count
                g_value_init(&value, G_TYPE_UINT);
                int i = 0;
                gchar *con = g_array_index(condition.contents, char*, i);
                guint number = atoi(con);
                printf("count %d\n",number);
                g_value_set_uint(&value, number);
                object_class->set_property(row_model, 0, &value, NULL);
                printf("param %d : %s\n", i, con);
                g_value_unset(&value);

                //初始化参数start
                g_value_init(&value, G_TYPE_UINT);
                i = 1;
                con = g_array_index(condition.contents, char*, i);
                number = atoi(con);
                printf("start %d\n",number);
                g_value_set_uint(&value, number);
                object_class->set_property(row_model, 1, &value, NULL);
                printf("param %d : %s\n", i, con);
                g_value_unset(&value);

                //初始化参数step
                g_value_init(&value, G_TYPE_UINT);
                i = 2;
                con = g_array_index(condition.contents, char*, i);
                number = atoi(con);
                printf("step %d\n",number);
                g_value_set_uint(&value, number);
                object_class->set_property(row_model, 2, &value, NULL);
                printf("param %d : %s\n", i, con);
                g_value_unset(&value);

                //初始化参数model
                g_value_init(&value, G_TYPE_UINT);
                number = 0;
                printf("model %d\n",number);
                g_value_set_uint(&value, number);
                object_class->set_property(row_model, 4, &value, NULL);
                printf("param %d : %s\n", i, con);
                g_value_unset(&value);
            }
        }else
        {
            printf("loop-while fail!\n");
        }
        printf("content fill completed\n");
        return row_model;
    }else if (g_strcmp0(name,"loop-until") == 0)
    {
        printf("set loop-until\n");

        type = TEP_TYPE_LOOP;
        row_model = tep_get_row_model_by_type(type);
        GObjectClass *object_class = G_OBJECT_GET_CLASS(row_model);

        if(xmlNode.is_parms)
        {
            if(xmlNode.contents->len == 3) {

                GValue value = G_VALUE_INIT;

                for (int i = index; i < xml_nodes->len; ++i) {
                    XML_NODE next = g_array_index(xml_nodes,XML_NODE,i);

                    if( g_strcmp0(next.name,"condition") == 0 && next.deep-1 == xmlNode.deep)
                    {
                        //初始化参数condition
                        g_value_init(&value, G_TYPE_STRING);

                        g_value_set_string(&value, next.content->str);
                        object_class->set_property(row_model, 3, &value, NULL);
                        printf("set loop until condition %d\n", g_value_get_string( &value));

                        g_array_remove_index(xml_nodes,i);
                        printf("loop until remove condition\n");
                        g_value_unset(&value);
                        break;
                    }
                }

                //初始化参数count
                g_value_init(&value, G_TYPE_UINT);
                int i = 0;
                gchar *con = g_array_index(xmlNode.contents, char*, i);
                guint number = atoi(con);
                printf("count %d\n",number);
                g_value_set_uint(&value, number);
                object_class->set_property(row_model, 0, &value, NULL);
                printf("param %d : %s\n", i, con);
                g_value_unset(&value);

                //初始化参数start
                g_value_init(&value, G_TYPE_UINT);
                i = 1;
                con = g_array_index(xmlNode.contents, char*, i);
                number = atoi(con);
                printf("start %d\n",number);
                g_value_set_uint(&value, number);
                object_class->set_property(row_model, 1, &value, NULL);
                printf("param %d : %s\n", i, con);
                g_value_unset(&value);

                //初始化参数step
                g_value_init(&value, G_TYPE_UINT);
                i = 2;
                con = g_array_index(xmlNode.contents, char*, i);
                number = atoi(con);
                printf("step %d\n",number);
                g_value_set_uint(&value, number);
                object_class->set_property(row_model, 2, &value, NULL);
                printf("param %d : %s\n", i, con);
                g_value_unset(&value);

                //初始化参数model
                g_value_init(&value, G_TYPE_UINT);
                number = 1;
                printf("model %d\n",number);
                g_value_set_uint(&value, number);
                object_class->set_property(row_model, 4, &value, NULL);
                printf("param %d : %s\n", i, con);
                g_value_unset(&value);
            }
        }else
        {
            printf("loop-while fail!\n");
        }
        printf("content fill completed\n");
        return row_model;
    }else if (g_strcmp0(name,"break") == 0)
    {
        type = TEP_TYPE_BREAK;
        row_model = tep_get_row_model_by_type(type);
        return row_model;
    }
    else if (g_strcmp0(name,"react-on") == 0)
    {
        type = TEP_TYPE_REACT_ON;
    }else if (g_strcmp0(name,"wait") == 0)
    {
        type = TEP_TYPE_WAIT;
    }else if (g_strcmp0(name,"try") == 0)
    {
        type = TEP_TYPE_TRY_CATCH;
    }else if (g_strcmp0(name,"catch") == 0)
    {
        printf("type = catch \n");
        type = TEP_TYPE_BASIC_BLOCK;
        row_model = tep_get_row_model_by_type(type);
        tep_basic_block_set_nick_name(row_model, "catch");

        GtkTreeIter try_iter = g_array_index(iter_array,GtkTreeIter,xmlNode.deep-1);

        gpointer try;
        gtk_tree_model_get(model, &try_iter, COL_ROW_MODEL, &try, -1);

        GValue value1 = G_VALUE_INIT;
        g_value_init(&value1,G_TYPE_STRING);
        gchar* con = g_array_index(xmlNode.contents,char*,0);
        g_value_set_string(&value1,con);
        GObjectClass *object_class = G_OBJECT_GET_CLASS(try);
        object_class->set_property(try,0,&value1,NULL);
        printf("param %d : %s\n",0,con);
        return row_model;
    }else if (g_strcmp0(name,"finally") == 0)
    {
        printf("type = finally \n");
        type = TEP_TYPE_BASIC_BLOCK;
        row_model = tep_get_row_model_by_type(type);
        tep_basic_block_set_nick_name(row_model, "finally");
        return row_model;
    }else if (g_strcmp0(name,"switch") == 0)
    {
        type = TEP_TYPE_SWITCH;
        XML_NODE node = g_array_index(xml_nodes,XML_NODE ,index);
        if(g_strcmp0(node.name,"condition") == 0)
        {
            GValue value = G_VALUE_INIT;
            row_model = tep_get_row_model_by_type(type);
            GObjectClass *object_class = G_OBJECT_GET_CLASS(row_model);

            g_value_init(&value,G_TYPE_STRING);
            g_value_set_string(&value,node.content->str);
            object_class->set_property(row_model,0,&value,NULL);
            printf("condition  %s\n", g_value_get_string(&value));
            g_array_remove_index(xml_nodes,index);
            printf("delete index %d \n",index);
            g_value_unset(&value);
            return row_model;
        }

    }else if (g_strcmp0(name,"case") == 0)
    {
        type = TEP_TYPE_SWITCH_CASE;
    }else if (g_strcmp0(name,"default") == 0)
    {
        printf("type = default \n");
        type = TEP_TYPE_BASIC_BLOCK;
        row_model = tep_get_row_model_by_type(type);
        tep_basic_block_set_nick_name(row_model, "default");
        return row_model;
    }else if(g_strcmp0(name,"execute") == 0)
    {
        printf("type = execute \n");
        type = TEP_TYPE_EXECUTE;
        char *execute_mess = g_string_new("");
        execute_mess = g_array_index(xmlNode.contents,char*,0);
        drag_component_name = execute_mess;
        printf("execute name = %s \n",drag_component_name);
        set_elements_drag_component_name();

        row_model = tep_get_row_model_by_type(type);
        GObjectClass *object_class = G_OBJECT_GET_CLASS(row_model);
        GValue value = G_VALUE_INIT;

        char* params = "";
        for (int i = 1; i < xmlNode.contents->len; ++i) {
            execute_mess = g_array_index(xmlNode.contents,char*,i);
            if (i == 1)
            {
                params = execute_mess;
            }else
            {
                params = g_strdup_printf("%s,%s",params,execute_mess);
            }
        }
        printf("params = %s\n",params);

        g_value_init(&value,G_TYPE_STRING);

        g_value_set_string(&value,params);
        object_class->set_property(row_model,2,&value,NULL);
//        g_array_remove_index(xml_nodes,index);
        g_value_unset(&value);

        XML_NODE node = g_array_index(xml_nodes,XML_NODE ,index );
        if(g_strcmp0(node.name,"return") == 0)
        {
            g_value_init(&value,G_TYPE_STRING);

            g_value_set_string(&value,node.content->str);
            object_class->set_property(row_model,4,&value,NULL);
            printf("return  %s\n", g_value_get_string(&value));
            g_array_remove_index(xml_nodes,index);
            printf("delete index %d \n",index);
            g_value_unset(&value);
        }
        return row_model;
    }
    else{
        printf("找不到类，默认设置为block\n");
        type = TEP_TYPE_BASIC_BLOCK;
    };

    printf("参数处理\n");
    //参数处理
    row_model = tep_get_row_model_by_type(type);
    GObjectClass *object_class = G_OBJECT_GET_CLASS(row_model);

    GValue value = G_VALUE_INIT;

    if(xmlNode.is_parms)
    {
        for (int i = 0; i < xmlNode.contents->len; ++i)
        {
            g_value_init(&value,G_TYPE_STRING);


            gchar* con = g_array_index(xmlNode.contents,char*,i);
            g_value_set_string(&value,con);
            object_class->set_property(row_model,i,&value,NULL);
            printf("param %d : %s\n",i,con);
        }
    }else
    {
        g_value_init(&value,G_TYPE_STRING);
        printf("content %s\n",xmlNode.content->str);
        gchar* test = xmlNode.content->str;
        g_value_set_string(&value,test);
        object_class->set_property(row_model,0,&value,NULL);
    }
    printf("content fill completed\n");
    g_value_unset(&value);

    return row_model;
}


/**
 * 解析xml name 的 类型
 */
GType function_name_get_type(char* name)
{
    GType type;
    if(g_strcmp0(name,"block") == 0)
    {
        type = TEP_TYPE_BASIC_BLOCK;
    } else if (g_strcmp0(name,"comment") == 0)
    {
        type = TEP_TYPE_COMMENT;
    }else if (g_strcmp0(name,"continue") == 0)
    {
        type = TEP_TYPE_CONTINUE;
    }else if (g_strcmp0(name,"emit") == 0)
    {
        type = TEP_TYPE_EMIT;
    }else if (g_strcmp0(name,"execute") == 0)
    {
        type = TEP_TYPE_EXECUTE;
    }else if (g_strcmp0(name,"exit") == 0)
    {
        type = TEP_TYPE_EXIT;
    }else if (g_strcmp0(name,"global") == 0)
    {
        type = TEP_TYPE_GLOBAL;
    }else if (g_strcmp0(name,"variable") == 0)
    {
        type = TEP_TYPE_VARIABLE;
    }
    else if (g_strcmp0(name,"if-then-else") == 0)
    {
        type = TEP_TYPE_IF_ELSE;
    }else if (g_strcmp0(name,"thread") == 0)
    {
        type = TEP_TYPE_THREAD;
    }
    else if (g_strcmp0(name,"join") == 0)
    {
        type = TEP_TYPE_JOIN;
    }else if (g_strcmp0(name,"loop-for") == 0)
    {
        type = TEP_TYPE_LOOP;
    }else if (g_strcmp0(name,"loop-while") == 0)
    {
        type = TEP_TYPE_LOOP;
    }else if (g_strcmp0(name,"loop-until") == 0)
    {
        type = TEP_TYPE_LOOP;
    }else if (g_strcmp0(name,"break") == 0)
    {
        type = TEP_TYPE_BREAK;
    }else if (g_strcmp0(name,"react-on") == 0)
    {
        type = TEP_TYPE_REACT_ON;
    }else if (g_strcmp0(name,"wait") == 0)
    {
        type = TEP_TYPE_WAIT;
    }else if (g_strcmp0(name,"try") == 0)
    {
        type = TEP_TYPE_TRY_CATCH;
    }else if (g_strcmp0(name,"switch") == 0)
    {
        type = TEP_TYPE_SWITCH;
    }else if (g_strcmp0(name,"case") == 0)
    {
        type = TEP_TYPE_SWITCH_CASE;
    }
    else{
        type = TEP_TYPE_BASIC_BLOCK;
    };

    return type;
}


char* xml_filename_get_content(char* path)
{
    FILE *file;
    char *buffer;
    long fileSize;

    // 打开文件
    file = fopen(path, "r");

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    rewind(file);

    // 分配内存来存储文件内容
    buffer = (char *)malloc(fileSize * sizeof(char));

    // 读取文件内容到缓冲区
    fread(buffer, sizeof(char), fileSize, file);

    // 输出文件内容
    printf("文件内容为 : \n%s\nend\n", buffer);
    // 关闭文件和释放内存
    fclose(file);

    return buffer;
}


void if_node_handel(XML_NODE xmlnode,GtkTreeModel *model,int index)
{
    GtkTreeIter iter = g_array_index(iter_array,GtkTreeIter,xmlnode.deep);
    GtkTreeIter child_iter;

    for (int i = index; i < xml_nodes->len ; ++i)
    {
        XML_NODE node = g_array_index(xml_nodes,XML_NODE,i);
        if(g_strcmp0(node.name,"block") == 0 && xmlnode.deep == node.deep-1)
        {
            child_iter = tep_insert_basic_block(GTK_TREE_STORE(model),&iter ,"Then");
            //更新iter
            printf("----11len = %d\n",iter_array->len);
            if(xmlnode.deep + 1 == iter_array->len)
            {
                g_array_append_val(iter_array,child_iter);
            }else{
                g_array_index(iter_array,GtkTreeIter,xmlnode.deep +1) = child_iter;
            }
            g_array_remove_index(xml_nodes,i);
            printf("create Then ,delete block, index = %d\n",i);
            index = i;
            break;
        }
    }

    for (int i = index; i < xml_nodes->len ; i++)
    {
        XML_NODE node = g_array_index(xml_nodes,XML_NODE,i);
        printf("then遍历 name %s deep %d \n",node.name,node.deep);
        if(g_strcmp0(node.name,"block") == 0 && (xmlnode.deep+1 == node.deep))
        {
            child_iter = tep_insert_basic_block(GTK_TREE_STORE(model),&iter ,"Else");
            g_array_remove_index(xml_nodes,i);
            //更新iter
            printf("----22len = %d\n",iter_array->len);
            if(xmlnode.deep +1 == iter_array->len)
            {
                g_array_append_val(iter_array,child_iter);
            }else{
                g_array_index(iter_array,GtkTreeIter,xmlnode.deep +1) = child_iter;
            }
            printf("delete block, index = %d\n",i);
            index = i;
            break;
        }else
        {
            //寻找then子节点
            GtkTreeIter iter2;
            gtk_tree_store_insert(GTK_TREE_STORE(model), &iter2,&(g_array_index(iter_array,GtkTreeIter,node.deep-1)), -1);

            printf("----33 1 len = %d\n",iter_array->len);
            if(node.deep  == iter_array->len)
            {
                g_array_append_val(iter_array,iter2);
            }else{
                g_array_index(iter_array,GtkTreeIter,node.deep) = iter2;
            }
            printf("----33 2 len = %d\n",iter_array->len);

            gpointer row_model =  function_name_get_gpointer(node,model,index);
            gtk_tree_store_set(GTK_TREE_STORE(model), &iter2, COL_ROW_MODEL, row_model, -1);

            GString* name = TEP_BASIC_COMPONENT(row_model)->action_name_renderer(row_model);
            printf("if then创建节点 %s\n", name->str);
            g_array_remove_index(xml_nodes,i);
            i--;
        }
    }

    //寻找else子节点
    for (int i = index; i < xml_nodes->len; ++i)
    {
        XML_NODE node = g_array_index(xml_nodes,XML_NODE,i);
        printf("else遍历 name %s deep %d \n",node.name,node.deep);
        if(xmlnode.deep < node.deep)
        {
            GtkTreeIter iter2;
            gtk_tree_store_insert(GTK_TREE_STORE(model), &iter2,&(g_array_index(iter_array,GtkTreeIter,node.deep-1)), -1);

            if(node.deep == iter_array->len)
            {
                g_array_append_val(iter_array,iter2);
            }else{
                g_array_index(iter_array,GtkTreeIter,node.deep) = iter2;
            }

            gpointer row_model =  function_name_get_gpointer(node,model,index);
            gtk_tree_store_set(GTK_TREE_STORE(model), &iter2, COL_ROW_MODEL, row_model, -1);

            GString* name = TEP_BASIC_COMPONENT(row_model)->action_name_renderer(row_model);
            printf("if else创建节点 %s\n", name->str);

            g_array_remove_index(xml_nodes,i);
            printf("------------------------------\n");
            i--;
            printf("------------------------------\n");
        }else{
            printf("if 遍历完成\n");
            break;
        }
    }
}


void node_handel(XML_NODE xmlnode,GtkTreeModel *model,int index)
{
    printf("node_handel :  %s\n",xmlnode.name);
    if(g_strcmp0(xmlnode.name,"if-then-else") == 0)
    {
        if_node_handel(xmlnode,model,index);
    }else if (g_strcmp0(xmlnode.name,"thread") == 0)
    {
        XML_NODE node = g_array_index(xml_nodes,XML_NODE,index);

        if(g_strcmp0(node.name,"block") == 0)
        {
            g_array_remove_index(xml_nodes,index);
        }
        node = g_array_index(xml_nodes,XML_NODE,index);
        for (int i = index; i < xml_nodes->len; ++i) {
            if(xmlnode.deep+1 < node.deep)
            {
                node.deep = node.deep -1;
                g_array_index(xml_nodes,XML_NODE,index) = node;
                node = g_array_index(xml_nodes,XML_NODE,++index);
            }else
            {
                printf("thread update end\n");
                break;
            }
        }
    }else if (g_strcmp0(xmlnode.name,"try") == 0)
    {
        XML_NODE node = g_array_index(xml_nodes,XML_NODE,index);
        if(g_strcmp0(node.name,"block") == 0 && (xmlnode.deep+1 == node.deep))
        {
            g_array_remove_index(xml_nodes,index);
            printf("delete %s\n",node.name);
        }
        GtkTreeIter child_iter = tep_insert_basic_block(GTK_TREE_STORE(model),&(g_array_index(iter_array,GtkTreeIter,xmlnode.deep)) ,"try");
        if(node.deep == iter_array->len)
        {
            g_array_append_val(iter_array,child_iter);
        }else{
            g_array_index(iter_array,GtkTreeIter,node.deep) = child_iter;
        }

        //寻找try子节点
        for (int i = index; i < xml_nodes->len; ++i)
        {
            XML_NODE node = g_array_index(xml_nodes,XML_NODE,i);
            printf("try遍历 name %s deep %d \n",node.name,node.deep);
            if(xmlnode.deep + 2 < node.deep)
            {
                int dep = 0;
                dep = node.deep - 1;
                node.deep = dep;
                g_array_index(xml_nodes,XML_NODE,i) = node;
                printf("设置deep为 %d\n",node.deep);
            }else{
                printf("try遍历 遍历完成\n");
                break;
            }
        }
    }else if(g_strcmp0(xmlnode.name,"catch") == 0)
    {
        XML_NODE node = g_array_index(xml_nodes,XML_NODE,index);
        if(g_strcmp0(node.name,"block") == 0 && (xmlnode.deep+1 == node.deep))
        {
            g_array_remove_index(xml_nodes,index);
            printf("delete %s\n",node.name);
        }

        //寻找try子节点
        for (int i = index; i < xml_nodes->len; ++i)
        {
            XML_NODE node = g_array_index(xml_nodes,XML_NODE,i);
            printf("catch遍历 name %s deep %d \n",node.name,node.deep);
            if(xmlnode.deep + 1 < node.deep)
            {
                int dep = 0;
                dep = node.deep - 1;
                node.deep = dep ;
                g_array_index(xml_nodes,XML_NODE,i) = node;
                printf("设置deep为 %d\n",node.deep);
            }else{
                printf("catch遍历 遍历完成\n");
                break;
            }
        }
    }else if(g_strcmp0(xmlnode.name,"finally") == 0)
    {
        XML_NODE node = g_array_index(xml_nodes,XML_NODE,index);
        if(g_strcmp0(node.name,"block") == 0 && (xmlnode.deep+1 == node.deep))
        {
            g_array_remove_index(xml_nodes,index);
            printf("delete %s\n",node.name);
        }

        //寻找try子节点
        for (int i = index; i < xml_nodes->len; ++i)
        {
            XML_NODE node = g_array_index(xml_nodes,XML_NODE,i);
            printf("try遍历 name %s deep %d \n",node.name,node.deep);
            if(xmlnode.deep + 1 < node.deep)
            {
                int dep = 0;
                dep = node.deep - 1;
                node.deep = dep;
                g_array_index(xml_nodes,XML_NODE,i) = node;
                printf("设置deep为 %d\n",node.deep);
            }else{
                printf("try遍历 遍历完成\n");
                break;
            }
        }
    }else if(g_strcmp0(xmlnode.name,"default") == 0)
    {
        XML_NODE node = g_array_index(xml_nodes,XML_NODE,index);
        if(g_strcmp0(node.name,"block") == 0 && (xmlnode.deep+1 == node.deep))
        {
            g_array_remove_index(xml_nodes,index);
            printf("delete %s\n",node.name);
        }

        //寻找default子节点
        for (int i = index; i < xml_nodes->len; ++i)
        {
            XML_NODE node = g_array_index(xml_nodes,XML_NODE,i);
            printf("default遍历 name %s deep %d \n",node.name,node.deep);
            if(xmlnode.deep + 1 < node.deep)
            {
                int dep = 0;
                dep = node.deep - 1;
                node.deep = dep;
                g_array_index(xml_nodes,XML_NODE,i) = node;
                printf("设置deep为 %d\n",node.deep);
            }else{
                printf("default遍历 遍历完成\n");
                break;
            }
        }
    }else if(g_strcmp0(xmlnode.name,"case") == 0)
    {
        XML_NODE node = g_array_index(xml_nodes,XML_NODE,index);
        if(g_strcmp0(node.name,"block") == 0 && (xmlnode.deep+1 == node.deep))
        {
            g_array_remove_index(xml_nodes,index);
            printf("delete %s\n",node.name);
        }

        //寻找case子节点
        for (int i = index; i < xml_nodes->len; ++i)
        {
            XML_NODE node = g_array_index(xml_nodes,XML_NODE,i);
            printf("case遍历 name %s deep %d \n",node.name,node.deep);
            if(xmlnode.deep + 1 < node.deep)
            {
                int dep = 0;
                dep = node.deep - 1;
                node.deep = dep;
                g_array_index(xml_nodes,XML_NODE,i) = node;
                printf("设置deep为 %d\n",node.deep);
            }else{
                printf("case遍历 遍历完成\n");
                break;
            }
        }
    }else if(g_strcmp0(xmlnode.name,"loop-while") == 0 ||
             g_strcmp0(xmlnode.name,"loop-until") == 0 ||
             g_strcmp0(xmlnode.name,"loop-for") == 0)
    {
        XML_NODE node = g_array_index(xml_nodes,XML_NODE,index);
        if(g_strcmp0(node.name,"block") == 0 && (xmlnode.deep+1 == node.deep))
        {
            g_array_remove_index(xml_nodes,index);
            printf("delete %s\n",node.name);
        }
        //寻找loop子节点
        for (int i = index; i < xml_nodes->len; ++i)
        {
            XML_NODE node = g_array_index(xml_nodes,XML_NODE,i);
            printf("loop遍历 name %s deep %d \n",node.name,node.deep);
            if(xmlnode.deep + 1 < node.deep)
            {
                int dep = 0;
                dep = node.deep - 1;
                node.deep = dep;
                g_array_index(xml_nodes,XML_NODE,i) = node;
                printf("设置deep为 %d\n",node.deep);
            }else{
                printf("loop遍历 遍历完成\n");
                break;
            }
        }
    }else if(g_strcmp0(xmlnode.name,"react-on") == 0)
    {
        XML_NODE node = g_array_index(xml_nodes,XML_NODE,index);
        if(g_strcmp0(node.name,"block") == 0 && (xmlnode.deep+1 == node.deep))
        {
            g_array_remove_index(xml_nodes,index);
            printf("delete %s\n",node.name);
        }

        //寻找case子节点
        for (int i = index; i < xml_nodes->len; ++i)
        {
            XML_NODE node = g_array_index(xml_nodes,XML_NODE,i);
            printf("react-on遍历 name %s deep %d \n",node.name,node.deep);
            if(xmlnode.deep + 1 < node.deep)
            {
                int dep = 0;
                dep = node.deep - 1;
                node.deep = dep;
                g_array_index(xml_nodes,XML_NODE,i) = node;
                printf("设置deep为 %d\n",node.deep);
            }else{
                printf("react-on遍历 遍历完成\n");
                break;
            }
        }
    }
}


int set_state(GtkTreeModel *model,GtkTreeIter iter)
{
    if(state->len == 0)
    {
        return 1;
    }
    GtkTreeIter children;
    if(gtk_tree_model_iter_children(model, &children, &iter) )
    {
        printf("find child \n");
        do
        {
            if(state->len == 0)
            {
                return 1;
            }
            gpointer row_model;
            gtk_tree_model_get(model, &children, COL_ROW_MODEL, &row_model, -1);
            TEP_BASIC_COMPONENT(row_model)->select = g_array_index(state,gboolean,0);
            g_array_remove_index(state,0);
            set_state(model,children);
        } while(gtk_tree_model_iter_next(model, &iter));
    }
}


/**
 * Export XML file
 */
char* tep_xml_get_tree(char* file_name,gboolean is_open,gchar* code,GtkTreeIter par)
{
    printf("start tep_xml_get_tree()\n");
    gchar *xml_data = "";
    char* filename = file_name;
    if(is_open)
    {
        xml_data = xml_filename_get_content(filename);
    }else
    {
        xml_data =code;
    }

    //文件内容预处理
    removeSpacesAndNewlines(xml_data,'\t');
    removeSpacesAndNewlines(xml_data,'\n');
    xml_data = insertMyBeforeFlag(xml_data,"break");
    xml_data = insertMyBeforeFlag(xml_data,"continue");
    xml_data = insertMyBeforeFlag(xml_data,"exit");
    removeSubstring(xml_data,"</parameters>");
    removeSubstring(xml_data,"<parameters>");
    removeSubstring(xml_data,"</parameter>");
    removeSubstring(xml_data,"<parameter>");
    removeSubstring(xml_data,"<returns>");
    removeSubstring(xml_data,"</returns>");
    printf("Modified String: %s\n", xml_data);

    GMarkupParseContext *context;
    GError *error = NULL;

    xml_nodes = g_array_new(FALSE,FALSE,sizeof(XML_NODE));
    state = g_array_new(FALSE,FALSE,sizeof(gboolean));
    iter_array = g_array_new(FALSE,FALSE,sizeof(GtkTreeIter));

    // 设置回调函数
    GMarkupParser parser = {
            .start_element = start_element_handler,
            .text = text_handler,
            .end_element = end_element_handler
    };
    int dep = 0;
    // 创建解析上下文
    context = g_markup_parse_context_new(&parser, 0, &dep, NULL);

    // 开始解析XML数据
    if(!g_markup_parse_context_parse(context, xml_data, -1, &error))
    {
        printf("%s\n",error->message);
        g_error_free(error);
    }

    // 释放资源
    g_markup_parse_context_free(context);

    //打印xml解析内容
    g_print("*** ------  xml node num = %d ,xml state len = %d ------ ***\n",xml_nodes->len,state->len);
    for(int i = 0;i<xml_nodes->len;i++)
    {
        XML_NODE xmlNode = g_array_index(xml_nodes,XML_NODE,i);
        printf("[ function name %s\n",xmlNode.name);
        printf("function deep  %d\n",xmlNode.deep);
        printf("function content  %s\n",xmlNode.content->str);
        printf("function contents len  %d ]\n",xmlNode.contents->len);

        if(xmlNode.contents->len !=0 )
        {
            for(int j = 0;j<xmlNode.contents->len;j++)
            {
                printf("params %d = %s\n",j, g_array_index(xmlNode.contents,char*,j));
            }
        }

        if(g_strcmp0(xmlNode.name,"execute") == 0)
        {
            if(!import_api_path_status)
            {
                printf("----1 len = %d\n",xml_nodes->len);
                g_array_free(xml_nodes,TRUE);
                printf("----2 len = %d\n",iter_array->len);
                g_array_free(iter_array,TRUE);
                printf("----3 len = %d\n",state->len);
                g_array_free(state,TRUE);
                show_message_dialog("导入 execute 节点");
                return "not import execute";
            }
        }
    }

    dep = 0;
    int index = 0;
    if(xml_nodes->len > 0)
    {
        //创建父节点
        GtkTreeIter iter1 ;
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(gen_case));
        if(is_open)
        {
            gtk_tree_store_insert(GTK_TREE_STORE(model), &iter1, NULL, -1);
        }else
        {
            if(gen_case_click_is_row)
            {
                gtk_tree_store_insert(GTK_TREE_STORE(model), &iter1, &par, -1);
            }else
            {
                gtk_tree_store_insert(GTK_TREE_STORE(model), &iter1, NULL, -1);
            }
        }
        g_array_append_val(iter_array,iter1);

        XML_NODE xmlNode = g_array_index(xml_nodes,XML_NODE,0);
        index = index + 1;
        printf("start 获取row model\n");
        gpointer first_model = function_name_get_gpointer(xmlNode,model,index);
        gtk_tree_store_set(GTK_TREE_STORE(model), &iter1, COL_ROW_MODEL, first_model, -1);

        GString *name = g_string_new("");
        name = TEP_BASIC_COMPONENT(first_model)->action_name_renderer(first_model);
        printf("创建第一个节点 %s\n", name->str);

        node_handel(xmlNode,model,index);

        while (index < xml_nodes->len)
        {
            xmlNode = g_array_index(xml_nodes,XML_NODE,index);
            index++;

            dep = xmlNode.deep;
            if(dep == 0)
            {
                GtkTreeIter iter2 ;
                if(is_open)
                {
                    gtk_tree_store_insert(GTK_TREE_STORE(model), &iter2, NULL, -1);
                }else
                {
                    if(gen_case_click_is_row)
                    {
                        gtk_tree_store_insert(GTK_TREE_STORE(model), &iter2, &par, -1);
                    }else
                    {
                        gtk_tree_store_insert(GTK_TREE_STORE(model), &iter2, NULL, -1);
                    }
                }
                g_array_index(iter_array,GtkTreeIter,0) = iter2;

                gpointer row_model = function_name_get_gpointer(xmlNode,model,index);
                gtk_tree_store_set(GTK_TREE_STORE(model), &iter2, COL_ROW_MODEL, row_model, -1);

                name = TEP_BASIC_COMPONENT(row_model)->action_name_renderer(row_model);
                printf("创建根节点 %s\n", name->str);

            }else
            {
                GtkTreeIter iter2;
                gtk_tree_store_insert(GTK_TREE_STORE(model), &iter2, &(g_array_index(iter_array,GtkTreeIter,dep-1)), -1);

                if(dep == iter_array->len)
                {
                    g_array_append_val(iter_array,iter2);
                }else{
                    g_array_index(iter_array,GtkTreeIter,dep) = iter2;
                }

                gpointer row_model =  function_name_get_gpointer(xmlNode,model,index);
                gtk_tree_store_set(GTK_TREE_STORE(model), &iter2, COL_ROW_MODEL, row_model, -1);

                name = TEP_BASIC_COMPONENT(row_model)->action_name_renderer(row_model);
                printf("创建子节点 %s deep = %d\n", name->str,xmlNode.deep);
            }
            node_handel(xmlNode,model,index);
        }

        if(state->len > 0)
        {
            TEP_BASIC_COMPONENT(first_model)->select = g_array_index(state,gboolean,0);
            g_array_remove_index(state,0);
            set_state(model,iter1);
        }
    }
    printf("----1 len = %d\n",xml_nodes->len);
    g_array_free(xml_nodes,TRUE);
    printf("----2 len = %d\n",iter_array->len);
    g_array_free(iter_array,TRUE);
    printf("----3 len = %d\n",state->len);
    g_array_free(state,TRUE);

}

/**
 * Export XML file
 */
void tep_open_testcase(GtkMenuItem *menuitem, gpointer user_data)
{
    // 创建文件选择对话框,PATH_SEPARATOR,"run.png")
    GtkWidget *dialog = gtk_file_chooser_dialog_new("选择要打开的文件",
                                                    GTK_WINDOW(tep_geany->geany_data->main_widgets->window),
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "Cancel",GTK_RESPONSE_CANCEL,
                                                    "Open",GTK_RESPONSE_ACCEPT,
                                                    NULL);

    // 设置对话框的默认按钮
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), g_strdup_printf("%s%s%s%s",current_pwd,PATH_SEPARATOR,"xml",PATH_SEPARATOR));

    // 运行文件选择对话框
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_ACCEPT)
    {
        // 获取用户选择的文件路径
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        g_print("选择的文件：%s\n", filename);
        default_open_file_path = g_strdup_printf("%s",filename);

        //清理树
        clear_tree_nodes();

        //解析xml
        GtkTreeIter invail_iter;
        tep_xml_get_tree(filename,TRUE,"",invail_iter);

        // 释放文件路径的内存
        g_free(filename);

        //保存打开的文件路径
        properities_file_save();

    }
    // 关闭对话框
    gtk_widget_destroy(dialog);

}
