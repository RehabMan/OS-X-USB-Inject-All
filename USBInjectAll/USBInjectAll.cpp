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

#include <libkern/version.h>
#include <IOKit/pci/IOPCIDevice.h>

#include "USBInjectAll.h"

//REVIEW: avoids problem with Xcode 5.1.0 where -dead_strip eliminates these required symbols
#include <libkern/OSKextLib.h>
void* _org_rehabman_dontstrip_[] =
{
    (void*)&OSKextGetCurrentIdentifier,
    (void*)&OSKextGetCurrentLoadTag,
    (void*)&OSKextGetCurrentVersionString,
};

static char g_exclude[256];
static char g_include[256];
static bool g_ignore_rmcf;
static bool g_exclude_xhc;

static char* g_exclude_hs;
static char* g_exclude_ss;

static char g_exclude_hs_static[] = "HS01,HS02,HS03,HS04,HS05,HS06,HS07,HS08,HS09,HS10,HS11,HS12,HS13,HS14";
static char g_exclude_ss_static[] = "SS01,SS02,SS03,SS04,SS05,SS06,SS07,SS08,SS09,SS10";

OSDefineMetaClassAndStructors(USBInjectAll, IOService)

static IOPCIDevice* getPCIDevice(IORegistryEntry* registryEntry)
{
    IOPCIDevice* pciDevice = NULL;
    while (registryEntry)
    {
        pciDevice = OSDynamicCast(IOPCIDevice, registryEntry);
        if (pciDevice)
            break;
        registryEntry = registryEntry->getParentEntry(gIOServicePlane);
    }
#ifdef DEBUG
    if (!pciDevice)
        AlwaysLog("getPCIDevice unable to find IOPCIDevice\n");
#endif
    return pciDevice;
}

OSDictionary* USBInjectAll::getConfigurationForController(UInt16 vendor, UInt16 device)
{
    USBInjectAll_config* configService = OSDynamicCast(USBInjectAll_config, waitForMatchingService(serviceMatching("USBInjectAll_config")));
    if (!configService)
    {
        AlwaysLog("USBInjectAll configuration service not available\n");
        return NULL;
    }

    OSDictionary* dict = configService->getConfiguration();
    if (!dict)
    {
        DebugLog("getConfigurationForController finds no Configuration dictionary\n");
        return NULL;
    }

    char key[32];
    OSDictionary* result = NULL;

    // try ConfigurationName from our own configuration
    if (OSString* configName = OSDynamicCast(OSString, getProperty("kName")))
    {
        DebugLog("trying '%s'\n", configName->getCStringNoCopy());
        result = OSDynamicCast(OSDictionary, dict->getObject(configName));
    }
    else if (OSString* configName = OSDynamicCast(OSString, getProperty("IONameMatched")))
    {
        DebugLog("trying '%s'\n", configName->getCStringNoCopy());
        result = OSDynamicCast(OSDictionary, dict->getObject(configName));
    }

    // try vvvv_dddd
    if (!result)
    {
        snprintf(key, sizeof(key), "%04x_%04x", vendor, device);
        DebugLog("trying '%s'\n", key);
        result = OSDynamicCast(OSDictionary, dict->getObject(key));
    }

    // try vvvv_ddxx
    if (!result)
    {
        snprintf(key, sizeof(key), "%04x_%02xxx", vendor, (device & 0xFF00) >> 8);
        DebugLog("trying '%s'\n", key);
        result = OSDynamicCast(OSDictionary, dict->getObject(key));
    }

    // try vvvv_dxxx
    if (!result)
    {
        snprintf(key, sizeof(key), "%04x_%01xxxx", vendor, (device & 0xF000) >> 12);
        DebugLog("trying '%s'\n", key);
        result = OSDynamicCast(OSDictionary, dict->getObject(key));
    }

    // try 'default'
    if (!result)
    {
        DebugLog("trying '%s'\n", "default");
        result = OSDynamicCast(OSDictionary, dict->getObject("default"));
    }

    if (!result)
        DebugLog("getConfigurationForController no matching configuration found\n");

    configService->release();

    return result;
}

static inline bool isDelimiter(char test)
{
    return ',' == test || ';' == test;
}

static void filterPorts(OSDictionary* ports, const char* filter, const char* keep)
{
    // filter is port names delimited by comma
    for (const char* p = filter; *p; )
    {
        char key[16];
        int count = sizeof(key)-1;
        char* dest = key;
        for (; count && *p && !isDelimiter(*p); --count)
            *dest++ = *p++;
        *dest = 0;
        if (!hack_strstr(keep, key))
        {
            DebugLog("removing port '%s'\n", key);
            ports->removeObject(key);
        }
        if (isDelimiter(*p)) ++p;
    }
}

OSDictionary* USBInjectAll::getConfiguration(UInt16 vendor, UInt16 device)
{
    // get configuration and make a copy
    OSDictionary* inject = getConfigurationForController(vendor, device);
    if (!inject)
        return NULL;
    OSDictionary* injectCopy = OSDynamicCast(OSDictionary, inject->copyCollection());
    if (!injectCopy)
    {
        DebugLog("copyCollection failed for injection properties\n");
        return NULL;
    }

    // filter based on uia_exclude, -uia_exclude_hs, -uia_exclude_ss
    if (OSDictionary* ports = OSDynamicCast(OSDictionary, injectCopy->getObject("ports")))
    {
        filterPorts(ports, g_exclude, g_include);
        if (g_exclude_hs)
            filterPorts(ports, g_exclude_hs, g_include);
        if (g_exclude_ss)
            filterPorts(ports, g_exclude_ss, g_include);
    }

    return injectCopy;
}

void USBInjectAll::injectProperties(IOService* provider, OSDictionary* inject, bool force)
{
    OSBoolean* disable = OSDynamicCast(OSBoolean, inject->getObject("Disabled"));
    if (disable && disable->isTrue())
    {
        provider->setProperty("RM,USBInjectAll_Disabled", true);
        return;
    }
    provider->setProperty("RM,USBInjectAll", true);
    if (OSCollectionIterator* iter = OSCollectionIterator::withCollection(inject))
    {
        // Note: OSDictionary always contains OSSymbol*
        while (const OSSymbol* key = static_cast<const OSSymbol*>(iter->getNextObject()))
        {
            // only overwrite existing properties when force is true
            if (force || !provider->getProperty(key))
            {
                if (OSObject* value = inject->getObject(key))
                    provider->setProperty(key, value);
            }
        }
        iter->release();
    }
}

IOService* USBInjectAll::probe(IOService* provider, SInt32* score)
{
    DebugLog("USBInjectAll::probe: Probing\n");

    if (!super::probe(provider, score))
        return NULL;

    // don't inject on XHC when -uia_exclude_xhc is specified
    if (g_exclude_xhc)
    {
        OSString* matched = OSDynamicCast(OSString, getProperty("IONameMatched"));
        if (matched && 0 == strncmp(matched->getCStringNoCopy(), "XHC", 3))
            return NULL;
        OSBoolean* isXHC = OSDynamicCast(OSBoolean, getProperty("kIsXHC"));
        if (isXHC && isXHC->isTrue())
            return NULL;
    }

    // determine vendor/device-id of the controller
    UInt32 dvID = 0;
    if (IOPCIDevice* pciDevice = getPCIDevice(provider))
    {
        // determine vendor/device-id of the controller
        dvID = pciDevice->extendedConfigRead32(kIOPCIConfigVendorID);
    }
    UInt16 vendor = dvID;
    UInt16 device = dvID >> 16;

    // get configuration and make a copy
    OSDictionary* inject = getConfiguration(vendor, device);
    if (!inject)
        return NULL;

#ifdef DEBUG
    provider->setProperty("RM,Configuration.Merged", inject);
    if (auto ioNameMatched = getProperty("IONameMatched"))
        provider->setProperty("RM,IONameMatched", ioNameMatched);
#endif
    // inject the configuration on the provider
    injectProperties(provider, inject, true);

    // cleanup and exit
    inject->release();
    return NULL; // short lived kext
}


OSDefineMetaClassAndStructors(USBInjectAll_config, IOService)

bool USBInjectAll_config::start(IOService* provider)
{
    if (!super::start(provider))
        return false;

    DebugLog("USBInjectAll_config starting...\n");

    // place version/build info in ioreg properties RM,Build and RM,Version
    char buf[128];
    snprintf(buf, sizeof(buf), "%s %s", OSKextGetCurrentIdentifier(), OSKextGetCurrentVersionString());
    setProperty("RM,Version", buf);
#ifdef DEBUG
    setProperty("RM,Build", "Debug-" LOGNAME);
#else
    setProperty("RM,Build", "Release-" LOGNAME);
#endif

    if (PE_parse_boot_argn("uia_exclude", g_exclude, sizeof g_exclude))
        AlwaysLog("uia_exclude specifies '%s'\n", g_exclude);

    if (PE_parse_boot_argn("uia_include", g_include, sizeof g_include))
        AlwaysLog("uia_include specifies '%s'\n", g_include);

    uint32_t flag;

    if (PE_parse_boot_argn("-uia_exclude_hs", &flag, sizeof flag))
    {
        g_exclude_hs = g_exclude_hs_static;
        AlwaysLog("-uia_exclude_hs specfies '%s'\n", g_exclude_hs);
    }

    if (PE_parse_boot_argn("-uia_exclude_ss", &flag, sizeof flag))
    {
        g_exclude_ss = g_exclude_ss_static;
        AlwaysLog("-uia_exclude_ss specifies '%s'\n", g_exclude_ss);
    }

    if (PE_parse_boot_argn("-uia_ignore_rmcf", &flag, sizeof flag))
    {
        g_ignore_rmcf = true;
        AlwaysLog("-uia_ignore_rmcf specified, will ignore ACPI RMCF customizations\n");
    }

    if (PE_parse_boot_argn("-uia_exclude_xhc", &flag, sizeof flag))
    {
        g_exclude_xhc = true;
        AlwaysLog("-uia_exclude_xhc specified, will not inject on XHC\n");
    }

    registerService();

    return true;
}

OSDictionary* USBInjectAll_config::getConfiguration()
{
    if (m_pConfiguration)
        return m_pConfiguration;

    OSDictionary* dict = OSDynamicCast(OSDictionary, getProperty("Configuration"));
    if (!dict)
    {
        DebugLog("no Configuration property found in USBInjectAll_config::start\n");
        return false;
    }

    OSDictionary* configCopy = OSDynamicCast(OSDictionary, dict->copyCollection());
    if (!configCopy)
    {
        DebugLog("could not make copy of configuration data\n");
        return false;
    }

    // allow overrides from ACPI at \UIAC.RMCF
    IOACPIPlatformDevice* acpi;
    if (!g_ignore_rmcf &&
        (acpi = OSDynamicCast(IOACPIPlatformDevice, IORegistryEntry::fromPath("IOService:/AppleACPIPlatformExpert/UIAC"))))
    {
        DebugLog("found override\n");
        if (OSObject* obj = getConfigurationOverride(acpi, "RMCF"))
        {
            DebugLog("RMCF returned something\n");
            if (OSDictionary* over = OSDynamicCast(OSDictionary, obj))
            {
#ifdef DEBUG
                setProperty("Configuration.Override", over);
#endif
                configCopy->merge(over);
                setProperty("Configuration", configCopy);
            }
            obj->release();
        }
    }

    m_pConfiguration = OSDynamicCast(OSDictionary, getProperty("Configuration"));
    return m_pConfiguration;
}

OSObject* USBInjectAll_config::translateEntry(OSObject* obj)
{
    // Note: non-NULL result is retained...

    // if object is another array, translate it
    if (OSArray* array = OSDynamicCast(OSArray, obj))
        return translateArray(array);

    // if object is a string, may be translated to boolean
    if (OSString* string = OSDynamicCast(OSString, obj))
    {
        // object is string, translate special boolean values
        const char* sz = string->getCStringNoCopy();
        if (sz[0] == '>')
        {
            // boolean types true/false
            if (sz[1] == 'y' && !sz[2])
                return OSBoolean::withBoolean(true);
            else if (sz[1] == 'n' && !sz[2])
                return OSBoolean::withBoolean(false);
            // escape case ('>>n' '>>y'), replace with just string '>n' '>y'
            else if (sz[1] == '>' && (sz[2] == 'y' || sz[2] == 'n') && !sz[3])
                return OSString::withCString(&sz[1]);
        }
    }
    return NULL; // no translation
}

OSObject* USBInjectAll_config::translateArray(OSArray* array)
{
    // may return either OSArray* or OSDictionary*

    int count = array->getCount();
    if (!count)
        return NULL;

    OSObject* result = array;

    // if first entry is an empty array, process as array, else dictionary
    OSArray* test = OSDynamicCast(OSArray, array->getObject(0));
    if (test && test->getCount() == 0)
    {
        // using same array, but translating it...
        array->retain();

        // remove bogus first entry
        array->removeObject(0);
        --count;

        // translate entries in the array
        for (int i = 0; i < count; ++i)
        {
            if (OSObject* obj = translateEntry(array->getObject(i)))
            {
                array->replaceObject(i, obj);
                obj->release();
            }
        }
    }
    else
    {
        // array is key/value pairs, so must be even
        if (count & 1)
            return NULL;

        // dictionary constructed to accomodate all pairs
        int size = count >> 1;
        if (!size) size = 1;
        OSDictionary* dict = OSDictionary::withCapacity(size);
        if (!dict)
            return NULL;

        // go through each entry two at a time, building the dictionary
        for (int i = 0; i < count; i += 2)
        {
            OSString* key = OSDynamicCast(OSString, array->getObject(i));
            // ignore empty entries which can occur at the end of an oversized package
            if (!key)
                continue;
            // get value, use translated value if translated
            OSObject* obj = array->getObject(i+1);
            OSObject* trans = translateEntry(obj);
            if (trans)
                obj = trans;
            dict->setObject(key, obj);
            OSSafeRelease(trans);
        }
        result = dict;
    }

    // Note: result is retained when returned...
    return result;
}

OSDictionary* USBInjectAll_config::getConfigurationOverride(IOACPIPlatformDevice* device, const char* method)
{
    // attempt to get configuration data from provider
    OSObject* r = NULL;
    if (kIOReturnSuccess != device->evaluateObject(method, &r))
        return NULL;

    // for translation method must return array
    OSObject* obj = NULL;
    OSArray* array = OSDynamicCast(OSArray, r);
    if (array)
        obj = translateArray(array);
    OSSafeRelease(r);

    // must be dictionary after translation, even though array is possible
    OSDictionary* result = OSDynamicCast(OSDictionary, obj);
    if (!result)
    {
        OSSafeRelease(obj);
        return NULL;
    }
    return result;
}

