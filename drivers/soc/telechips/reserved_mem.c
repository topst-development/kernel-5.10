// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/list_sort.h>
#include <linux/module.h>
#include <linux/of_reserved_mem.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

struct rmem_entry {
	const char *name;
	u64 base;
	u64 end;
	bool p_dynamic;
	bool p_nomap;
	bool p_reusable;
	struct list_head list;
};

static LIST_HEAD(rmem_list_head);

#define reserved_mem_flag(flag, prop) ((prop) ? (flag) : '-')

static int reserved_mem_show(struct seq_file *m, void *v)
{
	const struct rmem_entry *entry;

	list_for_each_entry(entry, &rmem_list_head, list) {
		seq_printf(m, "%llx-%llx %c%c%c %s\n",
			   entry->base,
			   entry->end,
			   reserved_mem_flag('d', entry->p_dynamic),
			   reserved_mem_flag('n', entry->p_nomap),
			   reserved_mem_flag('r', entry->p_reusable),
			   entry->name);
	}

	return 0;
}

DEFINE_PROC_SHOW_ATTRIBUTE(reserved_mem);

static int reserved_mem_compare(void *p, const struct list_head *a,
				const struct list_head *b)
{
	const struct rmem_entry *ra = list_entry(a, struct rmem_entry, list);
	const struct rmem_entry *rb = list_entry(b, struct rmem_entry, list);

	return (ra->base <= rb->base) ? -1 : 1;
}

static int reserved_mem_add_entry(struct device_node *node)
{
	const struct reserved_mem *rmem = of_reserved_mem_lookup(node);
	struct rmem_entry *entry = NULL;
	int ret = 0;

	if ((rmem != NULL) && (rmem->size > 0U)) {
		entry = kmalloc(sizeof(struct rmem_entry), GFP_KERNEL);
		ret = (entry == NULL) ? -ENOMEM : 0;
	}

	if (entry != NULL) {
		entry->name = rmem->name;
		entry->base = rmem->base;

		if ((PHYS_ADDR_MAX - rmem->base) < rmem->size) {
			entry->end = PHYS_ADDR_MAX;
		} else {
			entry->end = (u64)rmem->base + rmem->size - 1U;
		}

		entry->p_dynamic = !of_property_read_bool(node, "reg");
		entry->p_nomap = of_property_read_bool(node, "no-map");
		entry->p_reusable = of_property_read_bool(node, "reusable");

		list_add_tail(&entry->list, &rmem_list_head);
	}

	return ret;
}

static void reserved_mem_remove_list(void)
{
	struct rmem_entry *entry;
	struct rmem_entry *next;

	list_for_each_entry_safe(entry, next, &rmem_list_head, list) {
		list_del(&entry->list);
		kfree(entry);
	}
}

static int __init reserved_mem_init(void)
{
	int ret = 0;
	const struct device_node *np = NULL;
	struct device_node *rmem_node;
	const struct proc_dir_entry *pent;

	pent = proc_create("reserved_mem", 0444, NULL, &reserved_mem_proc_ops);
	if (pent == NULL) {
		ret = -ENOMEM;
	} else {
		np = of_find_node_by_name(NULL, "reserved-memory");
	}

	for_each_available_child_of_node(np, rmem_node) {
		ret = reserved_mem_add_entry(rmem_node);
		if (ret != 0) {
			break;
		}
	}

	if (ret == 0) {
		list_sort(NULL, &rmem_list_head, &reserved_mem_compare);
	} else {
		remove_proc_entry("reserved_mem", NULL);
		reserved_mem_remove_list();
	}

	return ret;
}

static void __exit reserved_mem_exit(void)
{
	remove_proc_entry("reserved_mem", NULL);
	reserved_mem_remove_list();
}

module_init(reserved_mem_init);
module_exit(reserved_mem_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("/proc/reserved_mem support");
