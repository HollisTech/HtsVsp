# HtsVsp
A windows user mode driver and application that provide a network virtual serial port.  
The intended use case is connecting WinDbg to xenserver and xcp-ng windows vms.  
The driver code is based on the Microsoft windows driver sample [VirtualSerial2](https://github.com/microsoft/Windows-driver-samples/tree/main/serial/VirtualSerial2)

## Building the code
The software was developed using Visual Studio 2022 and WDK 10.0.26100.1  
Other tested tool chains:

## Components
### UDF Driver
* HtsVsp.dll - the user mode serial driver.
* htsvsp.inf - the inf file for installing the driver.
* htsvsp.cat - the catalog file covering the preceding two files.
### Control application
* vspControl.exe

## Installation
There is no msi or other installer for the components. Instead all components should be copied to a directory on the system.  

The control application can be used to install and uninstall the driver.  
_vspControl --install .\htsvsp.inf_  
_vspControl --uninstall .\htsvsp.inf_  

**NOTE:** all windows drivers, even user mode, have to be signed. You must provide either a trusted signing certificate or a self signed certificate when building. User mode drivers do not require Microsoft certificates, unlike kernel mode drivers. (TBD: do self signed certs require test mode?)

## Configuring xcp-ng or xenserver for windows debugging
WIP  
**Note:** production systems should be avoided. 
### Overview
Xenserver and xcp-ng Xen implementations do not provide supported windows kernel debugging capabilities. The xenbus network device does not support kdnet debugging. The standard serial port on the host, if it exists, could be enabled as a passthrough device to a vm, but as typically the host is in a data center type environment, this is not particularly useful. The Xen host remote management port (serial over lan) could also be potentially passed through to a vm, which is slightly more feasible than a physical serial port, but is again a bit of a problem for a data center environment.  

What is feasible is to connect a Xen host tcp port to a vm serial port, and that actually is supported, if not fully documented. This results in a standard linux pty function. 

It is necessary to understand that the Xen host, not the target windows vm, provides the network pty. The vm has what appears to be and functions as a serial port.  The vm's Xen host has a tcp socket open for listening that, when connected, passes data through to the vm serial port. 

A Xen host can provide multiple ptys connected to multiple vms.  

The remaining problem is how to connect a windows system running windbg to this pty. That is what this project does. The driver, htsvsp.dll, is a usermode driver that emulates a serial port, with its lower edge input and output connected to a tcp port on a target host. Its upper edge provides a windows Comport API compatible device that can communicate with windows processes using the Comport API. As the focus is windows kernel debugging, the only standard windows process tested is WinDbg.

For use with windbg, configure the virtual serial port to connect to the Xen host at the correct tcp port for the target windows vm. Configure the target windows vm for serial kernel debugging. Configure WindDbg for kernel debugging using the comport provided by the virtual serial port.

### Process
A windows vm (the target vm) is going to be configured to have a serial port attached to a host tcp port. This results in a 'pty' connection on the host, and an emulated serial port on the vm.  
The host firewall configuration (iptables in current xenserver and xcp-ng releases) is going to be modified to open up a range of tcp ports, in this example ports 7001-7010. The vm is serial port is going to be connected to tcp port 7001. 
1. in a multihost pool configure the target vm so that it is attached to a specific pool host.
1. configure the xenserver host iptables to open up the port being used:  
edit /etc/sysconfig/iptables.  
Insert the line:  
_-A RH-Firewall-1-INPUT -p tcp -m tcp --dport 7001:7010 -j ACCEPT_
1. save the iptables and then restart the iptables service:  
_service iptables restart_
1. from the host command line configure the target vm to have a pseudo network serial port:  
_xe vm-param-set other-config:hvm_serial=tcp::7001,server,nodelay,nowait uuid=\<uuid>_
1. restart the target vm if it is currently running.

To remove a pty from a vm:  
_xe vm-param-remove param-name=other-config param-key=hvm_serial uuid=\<uuid>_  

To query the pty status for a vm:  
_xe vm-param-get uuid=\<uuid> param-name=platform param-key=hvm_serial_

