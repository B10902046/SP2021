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

int in[2][2], out[2][2];
char pid[100];
char buf[1024];
int tar_pid[2];
int fd, loser, winner, bid;
Status player[2];
static char* txt_names[14] = {"log_battleA.txt", "log_battleB.txt", "log_battleC.txt", "log_battleD.txt", "log_battleE.txt", "log_battleF.txt", "log_battleG.txt", "log_battleH.txt", "log_battleI.txt", "log_battleJ.txt", "log_battleK.txt", "log_battleL.txt", "log_battleM.txt", "log_battleN.txt"};

static char* tar_id[14][2] = {{"B", "C"}, {"D", "E"}, {"F", "14"}, {"G", "H"}, {"I", "J"}, {"K", "L"}, {"0", "1"}, {"2", "3"}, {"4", "5"}, {"6", "7"}, {"M", "10"}, {"N", "13"}, {"8", "9"}, {"11", "12"}};
static int field[14] = {0, 1, 2, 2, 0, 0, 0, 1, 2, 1, 1, 1, 0, 2};
static char par_id[14] = {'A', 'A', 'A', 'B', 'B', 'C', 'D', 'D', 'E', 'E', 'F', 'F', 'K', 'L'};

void cl_Pipe() {
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            close(in[i][j]);
            close(out[i][j]);
        }
    }
}

int main(int argc, char** argv) {
	bid = argv[1][0] - 'A';
    pipe(in[0]), pipe(in[1]), pipe(out[0]), pipe(out[1]);
    sprintf(pid, "%d", getpid());
    if((tar_pid[0]=fork()) == 0) {
        dup2(out[0][1], STDOUT_FILENO);
        dup2(in[0][0], STDIN_FILENO);
        cl_Pipe();
        if(isupper(tar_id[bid][0][0]))
            execlp("./battle", "battle", tar_id[bid][0], pid, NULL);
        else
            execlp("./player", "player", tar_id[bid][0], pid, NULL);
    }else if((tar_pid[1]=fork()) == 0) {
        dup2(out[1][1], STDOUT_FILENO);
        dup2(in[1][0], STDIN_FILENO);
        cl_Pipe();
        if(isupper(tar_id[bid][1][0]))
            execlp("./battle", "battle", tar_id[bid][1], pid, NULL);
        else
            execlp("./player", "player", tar_id[bid][1], pid, NULL);
    }else {
        fd = open(txt_names[bid], O_WRONLY|O_CREAT|O_APPEND, 0644);
        close(in[0][0]), close(in[1][0]), close(out[0][1]), close(out[1][1]);
        while (1) {
            read(out[0][0], player, sizeof(Status));
            sprintf(buf, "%c,%d pipe from %s,%d %d,%d,%c,%d\n", argv[1][0], getpid(), tar_id[bid][0], tar_pid[0], player[0].real_player_id, player[0].HP, player[0].current_battle_id, player[0].battle_ended_flag);
            write(fd, buf, strlen(buf));
            read(out[1][0], player + 1, sizeof(Status));
            sprintf(buf, "%c,%d pipe from %s,%d %d,%d,%c,%d\n", argv[1][0], getpid(), tar_id[bid][1], tar_pid[1], player[1].real_player_id, player[1].HP, player[1].current_battle_id, player[1].battle_ended_flag);
            write(fd, buf, strlen(buf));
            player[0].current_battle_id = player[1].current_battle_id = argv[1][0];
            if (player[0].HP < player[1].HP || (player[0].HP == player[1].HP && player[0].real_player_id < player[1].real_player_id)) {
                if (player[0].attr == field[bid]) {
                    player[1].HP -= 2 * player[0].ATK;
                }else {
                    player[1].HP -= player[0].ATK;
                }
                if (player[1].HP > 0) {
                    if (player[1].attr == field[bid]) {
                        player[0].HP -= 2 * player[1].ATK;
                    }else {
                        player[0].HP -= player[1].ATK;
                    }
                }
            }else {
                if (player[1].attr == field[bid]) {
                    player[0].HP -= 2 * player[1].ATK;
                }else {
                    player[0].HP -= player[1].ATK;
                }
                if (player[0].HP > 0) {
                    if (player[0].attr == field[bid]) {
                        player[1].HP -= 2 * player[0].ATK;
                    }else {
                        player[1].HP -= player[0].ATK;
                    }
                }
            }
            if (player[0].HP <= 0 || player[1].HP <= 0) {
                player[0].battle_ended_flag = player[1].battle_ended_flag = 1;
                if (player[0].HP <= 0)
                    loser = 0, winner = 1;
                else
                    loser = 1, winner = 0;
            }
            write(in[0][1], player, sizeof(Status));
            sprintf(buf, "%c,%d pipe to %s,%d %d,%d,%c,%d\n", argv[1][0], getpid(), tar_id[bid][0], tar_pid[0], player[0].real_player_id, player[0].HP, player[0].current_battle_id, player[0].battle_ended_flag);
            write(fd, buf, strlen(buf));
            write(in[1][1], player + 1, sizeof(Status));
            sprintf(buf, "%c,%d pipe to %s,%d %d,%d,%c,%d\n", argv[1][0], getpid(), tar_id[bid][1], tar_pid[1], player[1].real_player_id, player[1].HP, player[1].current_battle_id, player[1].battle_ended_flag);
            write(fd, buf, strlen(buf));
            if (player[0].battle_ended_flag)
                break;
        }
        wait(NULL);
        if (argv[1][0] == 'A') {
            printf("Champion is P%d\n", player[1-loser].real_player_id);
            wait(NULL);
            exit(0);
        }
        close(in[loser][1]), close(out[loser][0]);
        while(1) {
            read(out[winner][0], player + winner, sizeof(Status));
            sprintf(buf, "%c,%d pipe from %s,%d %d,%d,%c,%d\n", argv[1][0], getpid(), tar_id[bid][winner], tar_pid[winner], player[winner].real_player_id, player[winner].HP, player[winner].current_battle_id, player[winner].battle_ended_flag);
            write(fd, buf, strlen(buf));
            write(STDOUT_FILENO, player + winner, sizeof(Status));
            sprintf(buf, "%c,%d pipe to %c,%s %d,%d,%c,%d\n", argv[1][0], getpid(), par_id[bid], argv[2], player[winner].real_player_id, player[winner].HP, player[winner].current_battle_id, player[winner].battle_ended_flag);
            write(fd, buf, strlen(buf));
            read(STDIN_FILENO, player + winner, sizeof(Status));
            sprintf(buf, "%c,%d pipe from %c,%s %d,%d,%c,%d\n", argv[1][0], getpid(), par_id[bid], argv[2], player[winner].real_player_id, player[winner].HP, player[winner].current_battle_id, player[winner].battle_ended_flag);
            write(fd, buf, strlen(buf));
            write(in[winner][1], player + winner, sizeof(Status));
            sprintf(buf, "%c,%d pipe to %s,%d %d,%d,%c,%d\n", argv[1][0], getpid(), tar_id[bid][winner], tar_pid[winner], player[winner].real_player_id, player[winner].HP, player[winner].current_battle_id, player[winner].battle_ended_flag);
            write(fd, buf, strlen(buf));
            if (player[winner].HP <= 0 ||
                    player[winner].battle_ended_flag == 1 &&
                    player[winner].current_battle_id == 'A')
                        break;
        }
        wait(NULL);
        close(fd);
    }


	
	return 0;
}