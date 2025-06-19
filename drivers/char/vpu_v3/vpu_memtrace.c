/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (C) 2020 Telechips Inc.
 */
#include "vpu_comm.h"
#include "vpu_memtrace.h"

//#define ENABLE_MEM_TRACE
//#define SHOW_BLOCK_LIST
extern int get_memtrace_enable(void);

#define LIKELY(x)       			__builtin_expect((x),1)
#define UNLIKELY(x)     			__builtin_expect((x),0)

//#define PRINT_STATUS

//#define DBGT(fmt, args...) 	printk("[%s:%d]" fmt "\n", __FUNCTION__, __LINE__, ## args)
#define DBG(fmt, args...)	do { pr_err("[%s:%d]" fmt "\n", __FUNCTION__, __LINE__, ## args); } while(0)
#define DBG_ERR(fmt, args...)	do { pr_err("[%s:%d]" fmt "\n", __FUNCTION__, __LINE__, ## args); } while(0)

#define MEM_LOCK(m)				do{ mutex_lock(m);	} while(0)
#define MEM_UNLOCK(m)			do{ mutex_unlock(m); }while(0)

#define MEM_ALLOC(size)			kzalloc(size, GFP_KERNEL)
#define MEM_FREE(ptr)			kfree(ptr)

DEFINE_MUTEX(m_mutex);


#define MAX_FUNC_NAME_SIZE	256
//================================================================
// Linked List for Memory Checker
//================================================================
typedef struct MemoryBlock
{
	struct MemoryBlock* prev;
	struct MemoryBlock* next;

	char function[MAX_FUNC_NAME_SIZE];
	int line;
	int size;
	long address;
}MemoryBlock_t;

#if defined(ENABLE_MEM_TRACE)
static MemoryBlock_t* MEM_HEAD = NULL;
static MemoryBlock_t* MEM_TAIL = NULL;
#endif //#if defined(ENABLE_MEM_TRACE)

static unsigned int block_count;

#if defined(SHOW_BLOCK_LIST)
static void BlockPrint(void)
{
	MemoryBlock_t* tmp = MEM_HEAD;
	DBG("-------------------------------");
	while(tmp)
	{
		DBG("block:0x%x, address:0x%x", (int)tmp, (int)tmp->address);
		tmp = tmp->next;
	}
	DBG("-------------------------------\n");
}
#endif

#if defined(ENABLE_MEM_TRACE)
static void BlockAlloc(long address, int size, const char* function, int line)
{
	MemoryBlock_t* block = MEM_ALLOC(sizeof(MemoryBlock_t));

	if(block != NULL)
	{
		block->address = address;
		block->size = size;
		memset(block->function, 0x00, MAX_FUNC_NAME_SIZE);
		strncpy(block->function, function, MAX_FUNC_NAME_SIZE - 1);

		block->line = line;
		block->prev = NULL;
		block->next = NULL;

		if(UNLIKELY(MEM_HEAD == NULL))
		{
			MEM_HEAD = block;
			MEM_TAIL = MEM_HEAD;
		}
		else
		{
			block->prev = MEM_TAIL;
			MEM_TAIL->next = block;
			MEM_TAIL = block;
		}

#if defined(PRINT_STATUS)
		DBG(">>>ALLOC ptr:0x%x, size:%d, func:%s, line:%d\n", (int)block->address, block->size, block->function, block->line);
#endif
	}
	else
	{
		DBG("kzalloc failed, call from:%s:%d", function, line);
	}

#if defined(SHOW_BLOCK_LIST)
	BlockPrint();
#endif
}

static int BlockFree(long address)
{
	int free_count = 0;
	MemoryBlock_t* tmp = MEM_HEAD;

	while(tmp)
	{
		if(tmp->address == address)
		{
			if(tmp == MEM_TAIL)
			{
				MEM_TAIL = tmp->prev;
				//DBG("<<<< move TAIL to prev, %p", MEM_TAIL);
			}

			if(tmp == MEM_HEAD)
			{
				MEM_HEAD = tmp->next;
				//DBG(">>>> move HEAD to next, %p", MEM_HEAD);
			}

			if(tmp->prev)
				tmp->prev->next = tmp->next;

			if(tmp->next)
				tmp->next->prev = tmp->prev;

#if defined(PRINT_STATUS)
			DBG(">>>DEL ptr:0x%x, size:%d, func:%s, line:%d\n", (int)tmp->address, tmp->size, tmp->function, tmp->line);
#endif
			MEM_FREE(tmp);
			tmp = NULL;
			free_count = 1; // one memory freed
			break;
		}
		else
		{
			tmp = tmp->next;
		}
	}

#if defined(SHOW_BLOCK_LIST)
	BlockPrint();
#endif

	return free_count;
}

#define MAX_DISPLAY_LOG	30

static void BlockCheck(int* alloc_size, int* alloc_count, bool bDisplayLog)
{
	int count = 0;
	int size = 0;
	MemoryBlock_t* tmp = MEM_HEAD;

	if(bDisplayLog)
	{
		DBG("--------------------------------------------------------------------------------------------------");
	}

	while(tmp)
	{
		if((count < (MAX_DISPLAY_LOG)) && (bDisplayLog == true))
		{
			DBG(">>>ALLOC ptr:0x%x, size:%d, func:%s, line:%d", (int)tmp->address, tmp->size, tmp->function, tmp->line);
		}

		count++;
		size += tmp->size;
		tmp = (MemoryBlock_t*)tmp->next;
	}

	if(alloc_size)
	{
		*alloc_size = size;
	}

	if(alloc_count)
	{
		*alloc_count = count;
	}

	if(bDisplayLog)
	{
		if(count >= MAX_DISPLAY_LOG)
		{
			DBG(">>> leack count is greater than %d", MAX_DISPLAY_LOG);
		}

		DBG("--------------------------------------------------------------------------------------------------");
	}
}
#endif //ENABLE_MEM_TRACE


void* KALLOC(size_t size, const char* func_str, int func_line)
{
	void* this_ptr = NULL;
	this_ptr = MEM_ALLOC(size);

#ifdef ENABLE_MEM_TRACE
	if(get_memtrace_enable() == 1)
	{
		MEM_LOCK(&m_mutex);
		if(this_ptr)
		{
			BlockAlloc((long)this_ptr, size, func_str, func_line);
		}
		MEM_UNLOCK(&m_mutex);
	}
#endif

	if(this_ptr != NULL)
	{
		block_count++;
	}

	return this_ptr;
}

void KFREE(void *ptr, const char* func_str, int line)
{
	if(ptr)
	{
#ifdef ENABLE_MEM_TRACE
		if(get_memtrace_enable() == 1)
		{
			int free_cnt = 0;

			MEM_LOCK(&m_mutex);
			free_cnt = BlockFree((long)ptr);
			MEM_UNLOCK(&m_mutex);

			if(free_cnt == 0)
			{
				DBG(">>>> can not find memory %s:%d", func_str, line);
			}
		}
#endif
		MEM_FREE(ptr);
		block_count--;
	}
}

int VPU_mem_usage(void)
{
	DBG("VPU alloc count:%d", block_count);

#ifdef ENABLE_MEM_TRACE
	if(get_memtrace_enable() == 1)
	{
		int alloc_count = 0;
		int alloc_size = 0;

		MEM_LOCK(&m_mutex);
		BlockCheck(&alloc_size, &alloc_count, true);
		MEM_UNLOCK(&m_mutex);

		DBG("-------------------------------------------------------------------------------------");
		DBG("memory usage : nb_alloc:%d, %d byte", alloc_count, alloc_size);
		DBG("-------------------------------------------------------------------------------------");
		return alloc_size;
	}

	return 0;
#else
	return block_count;
#endif
}

int VPU_get_alloc_count(void)
{
#ifdef ENABLE_MEM_TRACE
	if(get_memtrace_enable() == 1)
	{
		int alloc_count = 0;
		int alloc_size = 0;

		MEM_LOCK(&m_mutex);
		BlockCheck(&alloc_size, &alloc_count, false);
		MEM_UNLOCK(&m_mutex);
		return alloc_count;
	}

	return 0;
#else
	return block_count;
#endif
}

EXPORT_SYMBOL(KALLOC);
EXPORT_SYMBOL(KFREE);
EXPORT_SYMBOL(VPU_mem_usage);
EXPORT_SYMBOL(VPU_get_alloc_count);

