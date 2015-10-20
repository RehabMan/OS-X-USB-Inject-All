// SSDT-UIAC.dsl
//
// This SSDT demonstrates a custom configuration for USBInjectAll.kext.
//

DefinitionBlock ("SSDT-UIAC.aml", "SSDT", 1, "hack", "UIAC", 0x00003000)
{
    Device(UIAC)
    {
        Name(_HID, "UIA00000")
        // override EH01 configuration to have only one port
        Name(RMCF, Package()
        {
            "EH01", Package()
            {
                "port-count", Buffer() { 1, 0, 0, 0 },
                "ports", Package()
                {
                    "PR11", Package()
                    {
                        "UsbConnector", 0,
                        "port", Buffer() { 1, 0, 0, 0 },
                    }
                }
            }
        })
    }
}

