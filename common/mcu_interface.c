/*
 * mcu_interface.c mcu interface of the vcu. implements access control to the
 * mailbox and the protocol to signal a message was sent to the mcu.
 *
 * Copyright (C) 2016, Sebastien Alaiwan (sebastien.alaiwan@allegrodvt.com)
 * Copyright (C) 2016, Kevin Grandemange (kevin.grandemange@allegrodvt.com)
 * Copyright (C) 2016, Antoine Gruzelle (antoine.gruzelle@allegrodvt.com)
 * Copyright (C) 2016, Allegro DVT (www.allegrodvt.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mcu_interface_private.h"
#include "mcu_utils.h"

#include <linux/printk.h>

int al5_mcu_interface_create(struct mcu_mailbox_interface **mcu,
			     struct device *device,
			     struct mcu_mailbox_config *config,
			     void *mcu_interrupt_register)
{
	*mcu = devm_kmalloc(device, sizeof(*mcu), GFP_KERNEL);
	(*mcu)->mcu_to_cpu = devm_kmalloc(device, sizeof(struct mailbox),
					  GFP_KERNEL);
	if (!(*mcu)->mcu_to_cpu)
		return -ENOMEM;

	(*mcu)->cpu_to_mcu = devm_kmalloc(device, sizeof(struct mailbox),
					  GFP_KERNEL);
	if (!(*mcu)->cpu_to_mcu)
		return -ENOMEM;

	al5_mailbox_init((*mcu)->cpu_to_mcu, (void *)config->cmd_base,
			 config->cmd_size);
	al5_mailbox_init((*mcu)->mcu_to_cpu, (void *)config->status_base,
			 config->status_size);
	spin_lock_init(&(*mcu)->read_lock);
	spin_lock_init(&(*mcu)->write_lock);
	(*mcu)->interrupt_register = mcu_interrupt_register;
	(*mcu)->dev = device;

	return 0;
}
EXPORT_SYMBOL_GPL(al5_mcu_interface_create);

void al5_mcu_interface_destroy(struct mcu_mailbox_interface *mcu,
			       struct device *device)
{
	devm_kfree(device, mcu->mcu_to_cpu);
	devm_kfree(device, mcu->cpu_to_mcu);
	devm_kfree(device, mcu);
}
EXPORT_SYMBOL_GPL(al5_mcu_interface_destroy);

/* These functions call mailbox methods */

int al5_mcu_send(struct mcu_mailbox_interface *mcu, struct al5_mail *mail)
{
	int error;

	spin_lock(&mcu->write_lock);
	error = al5_mailbox_write(mcu->cpu_to_mcu, mail);
	spin_unlock(&mcu->write_lock);

	if (error)
		dev_err_ratelimited(mcu->dev, "Cannot write in mailbox !");

	return error;
}
EXPORT_SYMBOL_GPL(al5_mcu_send);

struct al5_mail *al5_mcu_recv(struct mcu_mailbox_interface *mcu)
{
	struct al5_mail *mail;

	spin_lock(&mcu->read_lock);
	mail = al5_mailbox_read(mcu->mcu_to_cpu);
	spin_unlock(&mcu->read_lock);

	return mail;
}
EXPORT_SYMBOL_GPL(al5_mcu_recv);

int al5_mcu_is_empty(struct mcu_mailbox_interface *mcu)
{
	struct mailbox *mailbox = mcu->mcu_to_cpu;
	u32 head_value = ioread32(mailbox->head);
	u32 tail_value = ioread32(mailbox->tail);

	return head_value == tail_value;
}
EXPORT_SYMBOL_GPL(al5_mcu_is_empty);

void al5_signal_mcu(struct mcu_mailbox_interface *mcu)
{
	iowrite32(1, mcu->interrupt_register);
}
EXPORT_SYMBOL_GPL(al5_signal_mcu);

const u32 mcu_cache_offset = 0x80000000;

u32 al5_mcu_get_virtual_address(u32 physicalAddress)
{
	return physicalAddress + mcu_cache_offset;
}
EXPORT_SYMBOL_GPL(al5_mcu_get_virtual_address);
