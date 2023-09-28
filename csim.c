#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<string.h>
#include<stdbool.h>

/* 캐시 구조체 */

typedef struct cache{
	bool valid;	// valid bit
	bool dirty;	// dirty bit
	int tag;	// tag bit
	int a_time;	// access time 
	int fifo_time;	// fifo를 위한 block에 들어온 시간 
}cache;

cache *cp;

/* 전역변수들 */

int is_lru;		// 값이 0이면 random, 1이면 lru, 2이면 fifo
bool is_write_allocate; // 값이 1이면 write-allocate, 0이면 no-write-allocate
bool is_write_back;	// 값이 1이면 write back, 0이면 write-through
int l_count, l_miss;	// load(read) 연산 카운트
int s_count, s_miss;	// store(write) 연산 카운트
int c_cycle, m_cycle;	// memory에 접근하는 회수와 cache에 접근하는 회수

/* eviction을 진행하는 함수 */

int eviction(long addr_sets,int block_num)
{
	int victim, i; 
	int time_min = 987654321;
	cache *tmp = &cp[addr_sets*block_num];

	switch(is_lru){

		/* random */

		case 0: 
			return rand() % block_num;	// set당 가지고 있는 block의 index중 하나를 랜덤으로 리턴

		/* lru */

		case 1: 
			for(i=0;i<block_num;i++)	// 해당 set안의 block들을 검사해 접근한지 가장 오래된 block의 index를 찾고 리턴
			{
				if(tmp[i].a_time < time_min)
				{
					time_min = tmp[i].a_time;
					victim = i;
				}
			}
			break;

		/* fifo */

		case 2: 
			for(i=0;i<block_num;i++)
			{
				if(tmp[i].fifo_time < time_min)
				{
					time_min = tmp[i].fifo_time;
					victim = i;
				}
			}
			break;
	}
	return victim;
}

/* cache read 함수 */

void cache_read(int num_sets, int block_num, int block_size,long addr)
{
	long addr_sets, addr_tag;
	int i;
	int index = block_num;
	cache *tmp;
	l_count++;		// read count 세기

	addr_sets = (addr/block_size) % num_sets;	// 주소에서 set계산
	addr_tag = (addr/block_size) / num_sets;	// 주소에서 tag계산

	for(i=0;i<block_num;i++)
	{
		tmp = &cp[addr_sets*block_num + i];
		if(tmp->valid == true && tmp->tag == addr_tag)	// valid하고 tag가 일치할 경우 hit
		{
			tmp->a_time = c_cycle;
			return;
		}
		else if(tmp->valid == false)	// miss 발생 시 메모리에서 데이터를 읽어 와 저장할 index 찾기
		{
			if(index > i)
			{
				index = i;
			}
		}
	}
	
	/* 미스 발생 */

	l_miss++; 

	/* 캐시 내부에 빈공간이 없을 경우 */

	if(index == block_num)
	{
		index = eviction(addr_sets,block_num);	// 저장할 set의 index를 보내 줌 
		tmp = &cp[addr_sets*block_num+index];

		if (is_write_back && tmp->dirty) m_cycle++;	// write back일 경우 할당 해제와 동시에 메모리에 store
		tmp->valid = 1;
		tmp->a_time = c_cycle;	// 접근이 일어났으므로 시간 갱신
		tmp->tag = addr_tag;
		tmp->fifo_time = c_cycle;
		tmp->dirty = 0;		// 수정이 일어나진 않았으므로
		m_cycle++;		// 메모리에서 데이터를 load
	}

	/* 빈공간이 있을 경우 */

	else 
	{
		tmp = &cp[addr_sets*block_num+index];
		tmp->valid = 1;
		tmp->tag = addr_tag;
		tmp->fifo_time = c_cycle;
		tmp->a_time = c_cycle;	// 접근이 일어났으므로 시간 갱신
		m_cycle++;		// 메모리에서 데이터를 load
		tmp->dirty = 0;		// 수정이 일어나진 않았으므로
	}

}

/* cache write 함수 */

void cache_write(int num_sets, int block_num, int block_size, long addr)
{
	long addr_sets, addr_tag;
	int i;
	int index = block_num;
	cache *tmp;
	s_count++; //write count 세기

	addr_sets = (addr / block_size) % num_sets; // 주소에서 set계산
	addr_tag = (addr / block_size) / num_sets; // 주소에서 tag계산

	for (i = 0; i < block_num; i++)
	{
		tmp = &cp[addr_sets*block_num + i];
		if (tmp->valid == 1 && tmp->tag == addr_tag)	// valid하고 tag가 일치할 경우 hit
		{
			tmp->a_time = c_cycle;
			if (is_write_back == true) tmp->dirty = true; 	// write back일 경우 수정이 일어났으므로 dirty 설정
			else if (is_write_back == false) m_cycle++; // write through일 경우 바로 memory에 write
			c_cycle++;
			return;
		}
		else if (tmp->valid == 0) // miss 발생 시 메모리에서 데이터를 읽어 와 저장할 index 찾기
		{
			if (index > i)
			{
				index = i;
			}
		}
	}

	/* 미스 발생 */

	s_miss++;

	if(!is_write_allocate) // no write allocate일 경우 memory에만 write가 일어난다. 
	{	
		m_cycle++; 
		return; 	
	}

	/* 빈공간이 없을 경우 */

	if (index == block_num) // 캐시에 빈공간이 없다면
	{
		index = eviction(addr_sets, block_num); // 저장할 set의 index를 보내 줌 
		tmp = &cp[addr_sets*block_num + index];

		if (is_write_back && tmp->dirty) m_cycle++;	// dirty가 세팅되어있으면 지금 line이 대체되므로 memory에 write
		tmp->valid = 1;
		tmp->a_time = c_cycle;
		tmp->fifo_time = c_cycle;
		tmp->tag = addr_tag;
		tmp->dirty = 1;			// 수정이 일어났으므로 dirty 세팅
	}

	/* 빈공간이 있을 경우 */

	else 
	{
		tmp = &cp[addr_sets*block_num+index];
		tmp->valid = 1;
		tmp->tag = addr_tag;
		tmp->a_time = c_cycle;
		tmp->fifo_time = c_cycle;
		tmp->dirty = 1;
	}
	
	c_cycle++;	// write allocate일 경우 memory를 cache에 load하고 cache를 수정함
	m_cycle++;
}

/* 캐시 시뮬레이터 실행 파트 */

void cache_simulator(int num_sets, int block_num, int block_size)
{
	char func, address[16] = {0};
	int ignore;
	long int addr;

	/* 파일의 끝까지 한 줄씩 읽으면서 명령 실행 */

	while(scanf(" %c %s %d", &func, address, &ignore) != EOF) 
	{
		fflush(stdin);		// 입력 버퍼 초기화
		addr = strtol(address, NULL, 16); 

		switch (func) {
		case 'l':
			cache_read(num_sets, block_num, block_size, addr);
			c_cycle++;
			break;
		case 's':
			cache_write(num_sets, block_num, block_size, addr);
			break;
		}
	}

	/* 결과 출력 */

	int n = block_size/4;

	printf("Total loads :\t%d\n", l_count);
	printf("Total stores :\t%d\n", s_count);
	printf("Load hits :\t%d\n", l_count - l_miss);
	printf("Load misses :\t%d\n", l_miss);
	printf("Store hits :\t%d\n", s_count - s_miss);
	printf("Store miss :\t%d\n", s_miss);
	printf("Total cycles :\t%d\n", c_cycle+100*m_cycle*n);
}

int main(int argc,char* argv[])
{
	srand((unsigned int)time(NULL));
	long num_sets, block_num, block_size;
	num_sets = atoi(argv[1]); // sets의 개수
	block_num = atoi(argv[2]); // 한 set에 있는 block의 개수
	block_size = atoi(argv[3]); // block의 크기

	cp = (cache*)calloc(num_sets*block_num,sizeof(cache)); // block의 개수 * set의 개수만큼 공간 할당

	if(!strcmp(argv[4],"write-allocate")) is_write_allocate = true;
	if(!strcmp(argv[5],"write-back")) is_write_back = true;
	if(!strcmp(argv[6],"lru")) is_lru++;
	else if(!strcmp(argv[6],"fifo")) is_lru = 2;

	cache_simulator(num_sets,block_num,block_size); // 캐시 시뮬레이터 실행

	free(cp);
	return 0;
}
