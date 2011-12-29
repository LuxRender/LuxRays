/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include <sstream>

#include <boost/thread/thread.hpp>
#include <qt4/QtCore/qabstractitemmodel.h>

#include "luxrays/core/context.h"
#include "luxrays/core/device.h"
#include "luxrays/core/virtualdevice.h"

#include "hardwaretree.h"

//------------------------------------------------------------------------------
// Hardware tree view
//------------------------------------------------------------------------------

HardwareTreeItem::HardwareTreeItem(const QVariant &data, bool chkable, HardwareTreeItem *parent) {
	parentItem = parent;
	itemData = data;
	checkable = chkable;
}

HardwareTreeItem::~HardwareTreeItem() {
	qDeleteAll(childItems);
}

void HardwareTreeItem::appendChild(HardwareTreeItem *item) {
	item->parentItem = this;
	childItems.append(item);
}

HardwareTreeItem *HardwareTreeItem::child(int row) {
	return childItems.value(row);
}

int HardwareTreeItem::childCount() const {
	return childItems.count();
}

int HardwareTreeItem::columnCount() const {
	return 1;
}

QVariant HardwareTreeItem::data(int column) const {
	return itemData;
}

HardwareTreeItem *HardwareTreeItem::parent() {
	return parentItem;
}

int HardwareTreeItem::row() const {
	if (parentItem)
		return parentItem->childItems.indexOf(const_cast<HardwareTreeItem *>(this));

	return 0;
}

//------------------------------------------------------------------------------

HardwareTreeModel::HardwareTreeModel(const vector<DeviceDescription *> &devDescs) : QAbstractItemModel() {
	rootItem = new HardwareTreeItem("Hardware");

	// OpenCL devices
	HardwareTreeItem *oclDev = new HardwareTreeItem("OpenCL");
	rootItem->appendChild(oclDev);

	HardwareTreeItem *oclCPUDev = new HardwareTreeItem("CPUs");
	oclDev->appendChild(oclCPUDev);
	HardwareTreeItem *oclGPUDev = new HardwareTreeItem("GPUs and Accelerators");
	oclDev->appendChild(oclGPUDev);

#ifndef LUXRAYS_DISABLE_OPENCL
	for (size_t i = 0; i < devDescs.size(); ++i) {
		DeviceDescription *devDesc = devDescs[i];

		if (devDesc->GetType() ==  DEVICE_TYPE_OPENCL) {
			const OpenCLDeviceDescription *odevDesc = (OpenCLDeviceDescription *)devDesc;

			//HardwareTreeItem *newNode = new HardwareTreeItem(odevDesc->GetName().c_str(), true);
			HardwareTreeItem *newNode = new HardwareTreeItem(odevDesc->GetName().c_str());

			stringstream ss;
			cl::Platform platform = odevDesc->GetOCLDevice().getInfo<CL_DEVICE_PLATFORM>();
			ss << "Platform: " << platform.getInfo<CL_PLATFORM_VENDOR>();
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			ss.str("");
			ss << "Platform Version: " << platform.getInfo<CL_PLATFORM_VERSION>();
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			ss.str("");
			ss << "Type: ";
			switch (odevDesc->GetOCLDevice().getInfo<CL_DEVICE_TYPE>()) {
				case CL_DEVICE_TYPE_CPU:
					ss << "CPU";
					break;
				case CL_DEVICE_TYPE_GPU:
					ss << "GPU";
					break;
				case CL_DEVICE_TYPE_ACCELERATOR:
					ss << "ACCELERATOR";
					break;
				default:
					ss << "UNKNOWN";
					break;
			}
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			ss.str("");
			ss << "Compute Units: " << odevDesc->GetComputeUnits();
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			ss.str("");
			ss << "Clock: " << odevDesc->GetOCLDevice().getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>() << " MHz";
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			ss.str("");
			ss << "Max. Global Memory: " << (odevDesc->GetMaxMemory() / 1024) << " Kbytes";
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			ss.str("");
			ss << "Local Memory: " << (odevDesc->GetOCLDevice().getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() / 1024) << " Kbytes";
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			ss.str("");
			ss << "Max. Constant Memory: " << (odevDesc->GetOCLDevice().getInfo<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>() / 1024) << " Kbytes";
			newNode->appendChild(new HardwareTreeItem(ss.str().c_str()));

			if (odevDesc->GetOpenCLType() == OCL_DEVICE_TYPE_CPU)
				oclCPUDev->appendChild(newNode);
			else
				oclGPUDev->appendChild(newNode);
		}
	}
#endif
}

HardwareTreeModel::~HardwareTreeModel() {
	delete rootItem;
}

int HardwareTreeModel::columnCount(const QModelIndex &parent) const {
	if (parent.isValid())
		return static_cast<HardwareTreeItem *>(parent.internalPointer())->columnCount();
	else
		return rootItem->columnCount();
}

QVariant HardwareTreeModel::data(const QModelIndex &index, int role) const {
	if (!index.isValid())
		return QVariant();

	if (role == Qt::DisplayRole) {
		HardwareTreeItem *item = static_cast<HardwareTreeItem *>(index.internalPointer());

		return item->data(index.column());
	} else if (role ==  Qt::CheckStateRole) {
		HardwareTreeItem *item = static_cast<HardwareTreeItem *>(index.internalPointer());

		if (item->isCheckable())
			return Qt::Checked;
		else
			return QVariant();
	} else
		return QVariant();
}

QModelIndex HardwareTreeModel::index(int row, int column, const QModelIndex &parent) const {
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	HardwareTreeItem *parentItem;

	if (!parent.isValid())
		parentItem = rootItem;
	else
		parentItem = static_cast<HardwareTreeItem*>(parent.internalPointer());

	HardwareTreeItem *childItem = parentItem->child(row);
	if (childItem)
		return createIndex(row, column, childItem);
	else
		return QModelIndex();
}

QModelIndex HardwareTreeModel::parent(const QModelIndex &index) const {
	if (!index.isValid())
		return QModelIndex();

	HardwareTreeItem *childItem = static_cast<HardwareTreeItem*>(index.internalPointer());
	HardwareTreeItem *parentItem = childItem->parent();

	if (parentItem == rootItem)
		return QModelIndex();

	return createIndex(parentItem->row(), 0, parentItem);
}

int HardwareTreeModel::rowCount(const QModelIndex &parent) const {
	HardwareTreeItem *parentItem;
	if (parent.column() > 0)
		return 0;

	if (!parent.isValid())
		parentItem = rootItem;
	else
		parentItem = static_cast<HardwareTreeItem*>(parent.internalPointer());

	return parentItem->childCount();
}

Qt::ItemFlags HardwareTreeModel::flags(const QModelIndex &index) const {
	if (!index.isValid())
		return Qt::ItemIsSelectable | Qt::ItemIsEnabled;

	HardwareTreeItem *item = static_cast<HardwareTreeItem *>(index.internalPointer());

	if (item->isCheckable())
		return Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
	else
		return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}
