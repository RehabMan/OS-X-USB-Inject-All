## USBInjectAll.kext

In 10.11+ Apple has changed significantly the way the USB drivers work.  In the absense of a port injector, the drivers use ACPI to obtain information about which ports are active.  Often, this information is wrong.  Instead of correcting the DSDT, a port injector can be used (just as Apple did for their own computers).  But in order to create such an injector, you must first determine which ports are actually being used. And to do that you need to inject all ports so you can test all ports on the computer to determine which ones correspond to each available port address.  You can't test a port that is disabled...

That's where this kext comes in.

This kext attempts to inject all ports for each controller, and for hubs as well.  You can use this kext (temporarily) to enable all ports so you can determine which ports really need to be in the final injector.  Only the (potential) hub on EH01.PRT1 and EH02.PRT1 are injected.  Other hubs would require modifications.  So far, I haven't seen internal hubs connected to other ports.  The kext automatically determines the ports (and their addresses) based on the specifc USB controller chipsets.

EH01: 8-USB2 ports PR11-PR18.
EH02: 6-USB2 ports PR21-PR28.

EH01 hub: 8-USB2 ports HP11-HP18.
EH02 hub: 8-USB2 ports HP21-HP28.

XHC, 7-series chipset (8086:1e31): 4-USB2 ports HS01-HS04, 4-USB3 ports SSP5-SSP8.
XHC, 8/9-series chipset (8086:9xxx): 9-USB2 ports HS01-HS09, 6-USB3 ports SSP1-SSP6.
XHC, 8/9-series chipset (8086:8xxx): 14-USB2 ports HS01-HS14, 6-USB3 ports SSP1-SSP6.
XHC, 8/9-series chipset (8086:9cb1): 11-USB ports HS01-HS11, 4-USB3 ports SSP1-SSP4.
XHC, 100-series chipset (8086:a12f): 14-USB2 ports HS01-HS14, 10-USB3 ports SS01-SS10, plus USR1/USR2)
XHC, 100-series chipset (8086:9d2f): 10-USB2 ports HS01-HS10, 6-USB3 ports SS01-SS06, plus USR1/USR2)

This kext is only for 10.11+.  It has no use with prior versions.

Currently, only Intel controllers are supported.  The most commonly used SMBIOS model identifiers are in the kext.

Without a custom configuration, it is not intended that this kext be used long term.  It is best to create a custom injector containing only the ports that are active on the target machine, or to create an SSDT that customizes the port injection done by USBInjectAll.kext.  Customizing USBInjectAll.kext with an SSDT is covered later in this README.

For some chipsets, you may need to bypass the 15-port limit in 10.11.  In particular, XHCI controllers with device-id that starts with '8' will need the patch to bypass the limit.  The patch was created by arix98 and published on insanelymac.com here: http://www.insanelymac.com/forum/topic/308325-guide-1011-full-speed-usb-series-89-keeping-vanilla-sle/page-9#entry2175618 (post #179 of that thread).

This kext requires only 20 ports maximum, so the patch would be:

```
Comment: change 15 port limit to 20 in AppleUSBXHCIPCI
Name: AppleUSBXHCIPCI
Find: <83bd8cfe ffff10>
Replace: <83bd8cfe ffff15>
```

If you have a 100-series board, there can be up to 26 ports on XHCI, so you should increase the limit accordingly:
```
Comment: change 15 port limit to 26 in AppleUSBXHCIPCI
Name: AppleUSBXHCIPCI
Find: <83bd8cfe ffff10>
Replace: <83bd8cfe ffff1b>
```

You can copy/paste the patch from the file config_patches.plist that is checked into this project.  The config_patches.plist also contains the DSDT patches required to rename EHC1->EH01 and EHC2->EH02 which is important to avoid collisions between this kext and any built-in port injectors in the native kexts for your SMBIOS.

Note: Do not plan to use the patch long-term.  It could be problematic.  If you have more than 15 ports on XHC, you should use FakePCIID_XHCIMux to route the USB2 component of those USB3 ports to EHCI.  It is easy to stay under the limit if up to 8-USB2 ports are routed off XHC.

Currently, this kext does not support the latest 100 series boards.  It would be easy to add.

This kext may be helpful in installation scenarios as well where broken USB may prevent booting the installer from a USB stick.  It should work from EFI/Clover/kexts.


### Injected Property Customization

Two mechanisms are provided for customizing the injections that this kext performs.  Kernel flag 'uia_exclude' can be used to eliminate ports that would normally be injected.  uia_exclude takes is a string of characters with multiple port identifiers semi-colon delimited (comma also).

For example, on my Lenovo u430 without FakePCIID_XHCIMux, bluetooth is on XHC at HS06. And the touchscreen is at HS01.  I can disable the touchscreen by booting with kernel flag uia_exclude=HS01, or with uia_exclude=HS06 disable bluetooth.  To disable both, uia_exclude=HS01;HS06.  With FakePCIID_XHCIMux, the touchscreen is on the hub on USB port1 on EH01.  To disable this hub port, uia_exclude=HP11.  You can easily see which devices are connected to which ports.  Each port identifier injected by the kext is unique, so you can easily identify each one.

In addition a few other flags are available:

flag -uia_exclude_hs: excludes all HSxx ports

flag -uia_exclude_ss: excludes all SSxx ports

flag -uia_exclude_ssp: excludes all SSPx ports

flag -uia_exclude_xhc: disables injection on XHC


But excluding ports doesn't give all the flexibility that might be needed.  All of the data in the Info.plist for ConfigurationData can be configured through ACPI.

For example, if we wanted to enable only SSP1 on XHC for 8086_8xxx chipsets:

```
DefinitionBlock ("", "SSDT", 1, "hack", "UIAC", 0)
{
    Device(UIAC)
    {
        Name(_HID, "UIA00000")

        // override XHC configuration to have only one port
        Name(RMCF, Package()
        {
            "8086_8xxx", Package()
            {
                "port-count", Buffer() { 0xa, 0, 0, 0 },
                "ports", Package()
                {
                    "SSP1", Package()
                    {
                        "UsbConnector", 3,
                        "port", Buffer() { 0xa, 0, 0, 0 },
                    }
                }
            }
        })
    }
}
```

An example is also provided in SSDT-UIAC.dsl.  All of the data injected for each type of device can be changed via this mechnanism.

The SSDT-UIAC-ALL.dsl contains the same data present in the Info.plist.  Using it would result in a net zero change.  You can use it as a template to establish your own custom SSDT for the specific USB configuration on your computer.  Delete ports you do not need.  Or change UsbConnector or portType to match your own USB hardware configuration.  All XHC identifiers (vendor_device) are included, so you should probably start by eliminating the configurations that don't apply to your XHC device, leaving only the configuration for your device.  You can see your device-id in ioreg under the XHC node (vendor-id and device-id).


### Feedback

Please use this thread at tmx for futher details and feedback.

http://www.tonymacx86.com/el-capitan-laptop-support/173616-guide-10-11-usb-changes-solutions.html


### Downloads:

Downloads are available on Bitbucket:

https://bitbucket.org/RehabMan/os-x-usb-inject-all/downloads


The best way to download the config_patches.plist and other repo files is to download the project ZIP:

https://github.com/RehabMan/OS-X-USB-Inject-All/archive/706fea51222eb73343d347db10cf48500333a7bd.zip


If you need a copy of the repo with the 9-series injector:

https://github.com/RehabMan/OS-X-USB-Inject-All/archive/706fea51222eb73343d347db10cf48500333a7bd.zip


### How to Install

Install the kext with your favorite kext installer, such as Kext Wizard.

Or install via Terminal:
```
sudo cp -R Release/USBInjectAll.kext /Library/Extensions
sudo touch /System/Library/Extensions && sudo kextcache -u /
```

If you have a 9-series chipset XHC controller, 8086:8cb1, install XHCI-9-series.kext from the project as well.  The USB3 drivers will not load without this injector kext.  Update: As of 10.11.1, this injector is no longer needed as direct support was added.  If you need it, go back in history: https://github.com/RehabMan/OS-X-USB-Inject-All/tree/706fea51222eb73343d347db10cf48500333a7bd

If you have an X99-series chipset XHC controller, 8086:8d31, install XHCI-x99-injector.kext from the project as well. The USB3 drivers will not load without this injector kext.

Note: This kext assumes you already renamed EHC1->EH01 and EHC2->EH02.  It also assumes your XHCI controller is named XHC (not renamed to XHC1).  These names EH01/EH02/XHC are best to avoid conflicts with built-in port injectors for Apple computers.  Refer to config_patches.plist in this repo for the patches required (config_patches.plist/ACPI/DSDT/Patches).


### Build Environment

My build environment is currently Xcode 7.0, using SDK 10.11, targeting OS X 10.11.

Keep in mind the Info.plist is generated by generate_Info_plist.sh.  Do not edit the Info.plist directly.  USBInectAll_template-Info.plist serves as the starter Info.plist, with each model injected using USBInjectAll_model_template.plist.  This allows new models to be added easily by modifying the script.


### Change Log

2016-12-14 (0.5.16)

- add new MacBook Pro models: MacBookPro13,1, MacBookPro13,2, MacBookPro13,3


2016-12-13 (0.5.15)

- add more iMac models: iMac4,1, iMac4,2, iMac5,1, iMac6,1, iMac7,1, iMac8,1, iMac9,1, iMac10,1


2016-09-07 (0.5.14)

- add "model" for internal hub IOKitPersonality (IOKit matching has changed on 10.12)


2016-06-29 (0.5.12)

- add MacBookPro11,3 MacBooKPro11,4 MacBookPro11,5

- add MacBook9,1


2016-04-22 (0.5.11)

- allow "XHC" as match for the XHCI controller. The primary use for this would be in SSDT configuration as there is no XHC config in Info.plist


2016-03-30 (0.5.10)

- add special configuration for XHCI 8086:9cb1


2016-01-26 (0.5.9)

- add support for 100-series chipset XHCI 8086:9d2f


2015-11-17 (0.5.8)

- add kernel flags -uia_exclude_hs, -uia_exclude_ss, -uia_exclude_ssp


2015-11-16 (0.5.5)

- add kernel flag -uia_ignore_rmcf to allow ACPI RMCF customization to be ignored


2015-11-14 (0.5.4)

- fix bug with zero length dictionary in ACPI RMCF customization


2015-10-25 (0.5.3)

- initial 100-series support


2015-10-23 (0.5.2)

- add more SMBIOS (eg. MacPro*)


2015-10-20 (0.5.0)

- initial release

