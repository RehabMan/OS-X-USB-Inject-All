/*
 * Copyright (c) 2015 RehabMan. All rights reserved.
 *
 *  Released under "The GNU General Public License (GPL-2.0)"
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef __USBInjectAll__
#define __USBInjectAll__


#include <IOKit/IOService.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <IOKit/IOTimerEventSource.h>

#ifndef EXPORT
#define EXPORT __attribute__((visibility("default")))
#endif

#define AlwaysLog(args...) do { IOLog("USBInjectAll: " args); } while(0)
#ifdef DEBUG
#define DebugLog(args...) AlwaysLog(args)
#else
#define DebugLog(args...) do { } while(0)
#endif

class EXPORT USBInjectAll : public IOService
{
    typedef IOService super;
	OSDeclareDefaultStructors(USBInjectAll)

public:
    virtual IOService* probe(IOService* provider, SInt32* score);

protected:
    OSDictionary* getConfigurationForController(UInt16 vendor, UInt16 device);
    OSDictionary* getConfiguration(UInt16 vendor, UInt16 device);
    void injectProperties(IOService* provider, OSDictionary* inject, bool force);
};

class EXPORT USBInjectAll_config : public IOService
{
    typedef IOService super;
    OSDeclareDefaultStructors(USBInjectAll_config)

public:
    virtual bool start(IOService* provider);
    virtual OSDictionary* getConfiguration();

private:
    OSDictionary* m_pConfiguration = 0;

protected:
    OSDictionary* getConfigurationOverride(IOACPIPlatformDevice* device, const char* method);
    OSObject* translateArray(OSArray* array);
    OSObject* translateEntry(OSObject* obj);
};

#endif // __USBInjectAll__
