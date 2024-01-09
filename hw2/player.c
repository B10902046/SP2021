#include "status.h"
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

static char* txt_names[8] = {"log_player0.txt", "log_player1.txt", "log_player2.txt", "log_player3.txt", 
							"log_player4.txt", "log_player5.txt", "log_player6.txt", "log_player7.txt"};
static char* path_name[7] = {"player8.fifo", "player9.fifo", "player10.fifo", "player11.fifo", "player12.fifo", "player13.fifo", "player14.fifo"};

static char nextbid[15] = {'G', 'G', 'H', 'H', 'I', 'I', 'J', 'J', 'M', 'M', 'K', 'N', 'N', 'L', 'C'};
static int agent[14] = {-1, 14, -1, 10, 13, -1, 8, 11, 9, 12, -1, -1, -1, -1};
 
int attr_num(char a) {
	if (a == 'F') {
		return 0;
	}else if (a == 'W') {
		return 2;
	}else if (a == 'G') {
		return 1;
	}
	fprintf(stderr, "attr failed xd");
	return -1;
}

int main (int argc, char** argv) {
	int full_hp;
	int hp, atk, end_flag;
	char str[8];
	char battle_id;
	Status stat;
	int fd;
	int fd2;
	char buf[512];
	int id;
	sscanf(argv[1], "%d", &id);

	if (id < 8) {
		fd = open(txt_names[id], O_WRONLY|O_CREAT|O_APPEND, 0644);
		//perror("open");
		FILE* fp = fopen("player_status.txt", "r+");
		for (int i = 0; i < id; i++)
			fscanf(fp, "%d %d %s %c %d\n", &hp, &atk, str, &battle_id, &end_flag);
		fscanf(fp, "%d %d %s %c %d\n", &hp, &atk, str, &battle_id, &end_flag);
		full_hp = hp;
		stat.HP = hp;
		stat.ATK = atk;
		stat.battle_ended_flag = end_flag;
		stat.current_battle_id = battle_id;
		stat.real_player_id = id;
		stat.attr = attr_num(str[0]);
		//perror("initialize");
		while(1) {
			sprintf(buf, "%d,%d pipe to %c,%s %d,%d,%c,%d\n", id, getpid(), nextbid[id], argv[2], stat.real_player_id, stat.HP, stat.current_battle_id, stat.battle_ended_flag);
			write(fd, buf, strlen(buf));                    
			write(STDOUT_FILENO, &stat, sizeof(Status));
			read(STDIN_FILENO, &stat, sizeof(Status));
			sprintf(buf, "%d,%d pipe from %c,%s %d,%d,%c,%d\n", id, getpid(), nextbid[id], argv[2], stat.real_player_id, stat.HP, stat.current_battle_id, stat.battle_ended_flag);
			write(fd, buf, strlen(buf));
			if (stat.battle_ended_flag) {
				if(stat.current_battle_id == 'A')
					exit(0);
				if (stat.HP > 0) {
					stat.HP += (full_hp - stat.HP) / 2;
					stat.battle_ended_flag = 0;
				}else {
					stat.HP = full_hp;
					stat.battle_ended_flag = 0;
					int index = stat.current_battle_id - 'A';
					sprintf(buf, "%d,%d fifo to %d %d,%d\n", id, getpid(), agent[index], stat.real_player_id, stat.HP);
					write(fd, buf, strlen(buf));
					sleep(1);
					mkfifo(path_name[agent[index]-8], 0777);
					fd = open(path_name[agent[index]-8], O_WRONLY);
					write(fd, &stat, sizeof(Status));
					exit(0);
				}
			}
		}
	}else {
		mkfifo(path_name[id - 8], 0777);
		fd = open(path_name[id - 8], O_RDONLY, 0777);
		//perror("open fifo");
		read(fd, &stat, sizeof(Status));
		full_hp = stat.HP;
		fd2 = open(txt_names[stat.real_player_id], O_WRONLY|O_CREAT|O_APPEND, 0644);
		sprintf(buf, "%d,%d fifo from %d %d,%d\n", id, getpid(), stat.real_player_id, stat.real_player_id, stat.HP);
		write(fd2, buf, strlen(buf));
		while(1) {
			sprintf(buf, "%d,%d pipe to %c,%s %d,%d,%c,%d\n", id, getpid(), nextbid[id], argv[2], stat.real_player_id, stat.HP, stat.current_battle_id, stat.battle_ended_flag);
			write(fd2, buf, strlen(buf));
			write(STDOUT_FILENO, &stat, sizeof(Status));
			read(STDIN_FILENO, &stat, sizeof(Status));
			sprintf(buf, "%d,%d pipe from %c,%s %d,%d,%c,%d\n", id, getpid(), nextbid[id], argv[2], stat.real_player_id, stat.HP, stat.current_battle_id, stat.battle_ended_flag);
			write(fd2, buf, strlen(buf));
			if(stat.battle_ended_flag) {
				if (stat.current_battle_id == 'A') {
					exit(0);
				}
				if (stat.HP > 0) {
					stat.HP += (full_hp - stat.HP) / 2;
					stat.battle_ended_flag = 0;
				}else {
					exit(0);
				}
			}
		}
	}
	





	return 0;
}