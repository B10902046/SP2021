#include "header.h"

movie_profile* movies[MAX_MOVIES];
char *filter_movie[32][MAX_MOVIES];
double grade[32][MAX_MOVIES] = {};
unsigned int num_of_movies = 0;
unsigned int num_of_reqs = 0;
char *p_movie[MAX_MOVIES];
double *p_grade;
// Global request queue and pointer to front of queue
// TODO: critical section to protect the global resources
request* reqs[MAX_REQ];
int front = -1;

/* Note that the maximum number of processes per workstation user is 512. *
 * We recommend that using about <256 threads is enough in this homework. */
pthread_t tid[MAX_CPU][MAX_THREAD]; //tids for multithread

#ifdef PROCESS
pid_t pid[MAX_CPU][MAX_THREAD]; //pids for multiprocess
#endif

//mutex
pthread_mutex_t lock;

typedef struct piece{
        int start;
        int end;
        int req_id;
        int level;
}piece;

void initialize(FILE* fp);
request* read_request();
int pop();

int pop(){
        front+=1;
        return front;
}

void *mergesort(void *arg) {
    piece *piyan = (piece *) arg;
    if ((piyan->level > 4) || (piyan->end - piyan->start < 1024)) {
        sort(&filter_movie[piyan->req_id][piyan->start], &grade[piyan->req_id][piyan->start], piyan->end - piyan->start + 1);
    }else {
        piece lp, rp;
        lp.start = piyan->start;
        lp.end = (piyan->end + piyan->start) / 2;
        rp.start = lp.end + 1;
        rp.end = piyan->end;
        lp.level = rp.level = piyan->level + 1;
        lp.req_id = rp.req_id = piyan->req_id;
        pthread_t tidl, tidr;
		
        //pthread_create(&tidl, NULL, mergesort, (void *) &lp);
        pthread_create(&tidr, NULL, mergesort, (void *) &rp);
		mergesort(&lp);
        //pthread_join(tidl, NULL);
        pthread_join(tidr, NULL);
        char **tmp_movie = malloc((piyan->end - piyan->start + 1)*sizeof(char)*(MAX_LEN+1));
        double *tmp_grade = malloc((piyan->end - piyan->start + 1)*sizeof(double));
        int index = 0;
        int id = lp.req_id;
        while (lp.start <= lp.end && rp.start <= rp.end) {
            if ((grade[id][lp.start] > grade[id][rp.start]) || ((grade[id][lp.start] == grade[id][rp.start]) && (strcmp(filter_movie[id][lp.start], filter_movie[id][rp.start]) < 0))) {
                tmp_movie[index] = filter_movie[id][lp.start];
                tmp_grade[index] = grade[id][lp.start];
                index++;
                lp.start++;
            }else {
                tmp_movie[index] = filter_movie[id][rp.start];
                tmp_grade[index] = grade[id][rp.start];
                index++;
                rp.start++;
            }
        }
        while (lp.start <= lp.end) {
            tmp_movie[index] = filter_movie[id][lp.start];
            tmp_grade[index] = grade[id][lp.start];
            index++;
            lp.start++;
        }
        while (rp.start <= rp.end) {
            tmp_movie[index] = filter_movie[id][rp.start];
            tmp_grade[index] = grade[id][rp.start];
            index++;
            rp.start++;
        }
        index = 0;
        for (int i = piyan->start; i <= piyan->end; i++) {
            filter_movie[id][i] = tmp_movie[index];
            grade[id][i] = tmp_grade[index];
            index++;
        }
        free(tmp_movie);
        free(tmp_grade);
    }
}

void *filter(void *req_index){
    // filter
        int index = (int) req_index;
        int count = 0;
    for (int k = 0; k < num_of_movies; k++) {
        if ((strstr(movies[k]->title, reqs[index]->keywords) != NULL) || ((reqs[index]->keywords)[0] == '*')) {
            filter_movie[index][count] = movies[k]->title;
            for (int j = 0; j < NUM_OF_GENRE; j++) {
                grade[index][count] += movies[k]->profile[j] * reqs[index]->profile[j];
            }
            count++;
        }
    }
    // sort
    piece p1;
    p1.start = 0;
    p1.end = count - 1;
    p1.level = 0;
    p1.req_id = index;
    pthread_t tiddd;
    pthread_create(&tiddd, NULL, mergesort, (void *) &p1);
    pthread_join(tiddd, NULL);
    // output
    char buf[100] = {};
    sprintf(buf, "%dt.out", reqs[index]->id);
        FILE *fp;
        fp = fopen(buf, "w");
        for (int k = 0; k < count; k++) {
                fprintf(fp, "%s\n", filter_movie[index][k]);
        }
    fclose(fp);
        return NULL;
}

void pmerge(piece *p1) {
	int size = p1->end - p1->start + 1;
	char *tmp_movie[size];
	for(int i = 0; i < size; i++)
		tmp_movie[i] = malloc(MAX_LEN);
	double tmp_grade[size];
	if (p1->end - p1->start > 1024 && p1->level < 5) {
		pid_t pidl, pidr;
		if ((pidl=fork()) == 0) {
			piece *pleft = malloc(sizeof(piece));
			pleft->start = p1->start;
			pleft->end = (p1->end + p1->start) / 2;
			pleft->level = p1->level + 1;
			pmerge(pleft);
			exit(0);
		}
		if ((pidr=fork()) == 0) {
			piece *pright = malloc(sizeof(piece));
			pright->start = (p1->end + p1->start) / 2 + 1;
			pright->end = p1->end;
			pright->level = p1->level + 1;
			pmerge(pright);
			exit(0);
		}
		waitpid(pidl, NULL, 0);
		waitpid(pidr, NULL, 0);
		// merge
		
		int index = 0;
		int lstart = p1->start, lend = (p1->end + p1->start) / 2, rstart = (p1->end + p1->start) / 2 + 1, rend = p1->end;
		while (lstart <= lend && rstart <= rend) {
			if ((p_grade[lstart] > p_grade[rstart]) || ((p_grade[lstart] == p_grade[rstart])&&(strcmp(p_movie[lstart], p_movie[rstart]) < 0))) {
				strcpy(tmp_movie[index],p_movie[lstart]);
				tmp_grade[index]=p_grade[lstart];
				index++;
				lstart++;
			}else {
				strcpy(tmp_movie[index],p_movie[rstart]);
				tmp_grade[index]=p_grade[rstart];
				index++;
				rstart++;
			}
		}
		while (lstart <= lend) {
			strcpy(tmp_movie[index],p_movie[lstart]);
			tmp_grade[index]=p_grade[lstart];
			index++;
			lstart++;
		}
		while (rstart <= rend) {
			strcpy(tmp_movie[index],p_movie[rstart]);
			tmp_grade[index]=p_grade[rstart];
			index++;
			rstart++;
		}
		for (int i = 0; i < index; i++) {
			strcpy(p_movie[p1->start + i],tmp_movie[i]);
			p_grade[p1->start + i] = tmp_grade[i];
		}
			
	}else {
		for (int i = p1->start; i <= p1->end; i++) {
			strcpy(tmp_movie[i-p1->start], p_movie[i]);
			tmp_grade[i-p1->start] = p_grade[i];
		}
		
		sort(tmp_movie, tmp_grade, p1->end-p1->start+1);
		
		for (int i = p1->start; i <= p1->end; i++) {
			strcpy(p_movie[i],tmp_movie[i-p1->start]);
			p_grade[i] = tmp_grade[i-p1->start];
		}
		
	}
	for (int i = 0; i < size; i++)
		free(tmp_movie[i]);
}

int main(int argc, char *argv[]){

        if(argc != 1){
#ifdef PROCESS
                fprintf(stderr,"usage: ./pserver\n");
#elif defined THREAD
                fprintf(stderr,"usage: ./tserver\n");
#endif
                exit(-1);
        }

        FILE *fp;

        if ((fp = fopen("./data/movies.txt","r")) == NULL){
                ERR_EXIT("fopen");
        }

        initialize(fp);
        assert(fp != NULL);
        

#ifdef THREAD
		for (int j = 0; j < 32; j++) {
                for (int k = 0; k < MAX_MOVIES; k++) {
                        filter_movie[j][k] = malloc(sizeof(char)*(MAX_LEN+1));
                }
        }
        for (int k = 0; k < num_of_reqs; k++) {
                pthread_create(&tid[k][0], NULL, filter, (void *) k);
        }
        for (int k = 0; k < num_of_reqs; k++) {
                pthread_join(tid[k][0], NULL);
        }
#elif defined PROCESS
        for (int i = 0; i < num_of_movies; i++) {
            p_movie[i] = mmap(NULL, sizeof(char)*(MAX_LEN+1), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        }
        p_grade = mmap(NULL, sizeof(double)*(num_of_movies), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        // filter
		for (int i = 0; i < num_of_movies; i++)
			p_grade[i] = 0;
		
        int count = 0;
		for (int k = 0; k < num_of_movies; k++) {
			if ((strstr(movies[k]->title, reqs[0]->keywords) != NULL) || ((reqs[0]->keywords)[0] == '*')) {
				strcpy(p_movie[count], movies[k]->title);
				for (int j = 0; j < NUM_OF_GENRE; j++) {
					p_grade[count] += movies[k]->profile[j] * reqs[0]->profile[j];
				}
				count++;
			}
		}
		
        piece p1;
        p1.start = 0;
        p1.end = count - 1;
        p1.level = 0;
		
        pmerge(&p1);
		
        char buf[100] = {};
    	sprintf(buf, "%dp.out", reqs[0]->id);
        FILE *fp2;
        fp2 = fopen(buf, "w");
        for (int k = 0; k < count; k++) {
            fprintf(fp2, "%s\n", p_movie[k]);
        }
    	fclose(fp2);

#endif
        fclose(fp);

        return 0;
}

/**=======================================
 * You don't need to modify following code *
 * But feel free if needed.                *
 =========================================**/

request* read_request(){
        int id;
        char buf1[MAX_LEN],buf2[MAX_LEN];
        char delim[2] = ",";

        char *keywords;
        char *token, *ref_pts;
        char *ptr;
        double ret,sum;

        scanf("%u %254s %254s",&id,buf1,buf2);
        keywords = malloc(sizeof(char)*strlen(buf1)+1);
        if(keywords == NULL){
                ERR_EXIT("malloc");
        }

        memcpy(keywords, buf1, strlen(buf1));
        keywords[strlen(buf1)] = '\0';

        double* profile = malloc(sizeof(double)*NUM_OF_GENRE);
        if(profile == NULL){
                ERR_EXIT("malloc");
        }
        sum = 0;
        ref_pts = strtok(buf2,delim);
        for(int i = 0;i < NUM_OF_GENRE;i++){
                ret = strtod(ref_pts, &ptr);
                profile[i] = ret;
                sum += ret*ret;
                ref_pts = strtok(NULL,delim);
        }

        // normalize
        sum = sqrt(sum);
        for(int i = 0;i < NUM_OF_GENRE; i++){
                if(sum == 0)
                                profile[i] = 0;
                else
                                profile[i] /= sum;
        }

        request* r = malloc(sizeof(request));
        if(r == NULL){
                ERR_EXIT("malloc");
        }

        r->id = id;
        r->keywords = keywords;
        r->profile = profile;

        return r;
}

/*=================initialize the dataset=================*/
void initialize(FILE* fp){

        char chunk[MAX_LEN] = {0};
        char *token,*ptr;
        double ret,sum;
        int cnt = 0;

        assert(fp != NULL);

        // first row
        if(fgets(chunk,sizeof(chunk),fp) == NULL){
                ERR_EXIT("fgets");
        }

        memset(movies,0,sizeof(movie_profile*)*MAX_MOVIES);

        while(fgets(chunk,sizeof(chunk),fp) != NULL){

                assert(cnt < MAX_MOVIES);
                chunk[MAX_LEN-1] = '\0';

                const char delim1[2] = " ";
                const char delim2[2] = "{";
                const char delim3[2] = ",";
                unsigned int movieId;
                movieId = atoi(strtok(chunk,delim1));

                // title
                token = strtok(NULL,delim2);
                char* title = malloc(sizeof(char)*strlen(token)+1);
                if(title == NULL){
                        ERR_EXIT("malloc");
                }

                // title.strip()
                memcpy(title, token, strlen(token)-1);
                title[strlen(token)-1] = '\0';

                // genres
                double* profile = malloc(sizeof(double)*NUM_OF_GENRE);
                if(profile == NULL){
                        ERR_EXIT("malloc");
                }

                sum = 0;
                token = strtok(NULL,delim3);
                for(int i = 0; i < NUM_OF_GENRE; i++){
                        ret = strtod(token, &ptr);
                        profile[i] = ret;
                        sum += ret*ret;
                        token = strtok(NULL,delim3);
                }

                // normalize
                sum = sqrt(sum);
                for(int i = 0; i < NUM_OF_GENRE; i++){
                        if(sum == 0)
                                profile[i] = 0;
                        else
                                profile[i] /= sum;
                }

                movie_profile* m = malloc(sizeof(movie_profile));
                if(m == NULL){
                        ERR_EXIT("malloc");
                }

                m->movieId = movieId;
                m->title = title;
                m->profile = profile;

                movies[cnt++] = m;
        }
        num_of_movies = cnt;

        // request
        scanf("%d",&num_of_reqs);
        assert(num_of_reqs <= MAX_REQ);
        for(int i = 0; i < num_of_reqs; i++){
                reqs[i] = read_request();
        }
}
/*========================================================*/