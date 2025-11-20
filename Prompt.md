Have a look at the python implementation present in the folder Python Implementation. 
Read the BLE protocol handling files. 

i'm porting the development to ESP32 to bring a cloud indipendent local control solution that allow to toggle outputs (ac, dc) and monitor battery status locally without any cloud connection. 

Study deeply the implementation in python and find what are the differences and fix the bug.  

Investigate closely on how the Python code handles the connection, use notifications  and complete the implementation.(that is 100% working) 

At this point in time code compiles but is not able to keep the connection, we need to start debugging it (refer to the pyhton implementation) to connect and keep a connection active, and read out battery percentage. we can focus on this at the beginning. 

Since i did also implement a custom curve SECP160 under the mbedtls lib (because uECC lib creates linker errors) i'm not even sure about the crypto implementation of it. can you please double check read the logging along the way so that we can together debug this.

As for folder structure, in the folder you'll find the Python Implementation (working) for the Ecoflow BLE (it's just a clone of the original repo), code is instead in the EcoflowESP32 folder. To compile use platformio (install via pip).

Here the logs:

 NimBLEScan: Updated advertiser: 7c:2c:67:44:a4:3e
Found device
D NimBLEScan: >> stop()
D NimBLEScan: << stop()
State changed: 1
Connecting…
D NimBLEClient: >> connect(7c:2c:67:44:a4:3e)
D NimBLEClient: Got Client event 
I NimBLEClient: Connected event
D NimBLEClient: Got Client event 
D NimBLEClient: Peer requesting to update connection parameters
D NimBLEClient: MinInterval: 16, MaxInterval: 32, Latency: 0, Timeout: 400
D NimBLEClientCallbacks: onConnParamsUpdateRequest: default
D NimBLEClient: Accepted peer params
D NimBLEClient: Got Client event 
I NimBLEClient: mtu update event; conn_handle=1 mtu=255
I NimBLEClient: Connection established
D NimBLEClient: Got Client event 
I NimBLEClient: mtu update event; conn_handle=1 mtu=255
D NimBLEClient: >> deleteServices
D NimBLERemoteService: >> deleteCharacteristics
D NimBLERemoteCharacteristic: >> deleteDescriptors
D NimBLERemoteCharacteristic: << deleteDescriptors
D NimBLERemoteCharacteristic: >> deleteDescriptors
D NimBLERemoteCharacteristic: << deleteDescriptors
D NimBLERemoteService: << deleteCharacteristics
D NimBLEClient: << deleteServices
Connected to device
D NimBLEClient: >> getService: uuid: 00000001-0000-1000-8000-00805f9b34fb
D NimBLEClient: >> retrieveServices
D NimBLEClient: Service Discovered >> status: 14 handle: -1
D NimBLEClient: << Service Discovered
D NimBLEClient: << retrieveServices
D NimBLEClient: >> retrieveServices
D NimBLEClient: Service Discovered >> status: 0 handle: 14
D NimBLERemoteService: >> NimBLERemoteService()
D NimBLERemoteService: << NimBLERemoteService(): 0x0001
D NimBLEClient: Service Discovered >> status: 14 handle: -1
D NimBLEClient: << Service Discovered
D NimBLEClient: << retrieveServices
D NimBLERemoteService: >> getCharacteristic: uuid: 00000002-0000-1000-8000-00805f9b34fb
D NimBLERemoteService: >> retrieveCharacteristics() for service: 0x0001
D NimBLEClient: Got Client event 
I NimBLEClient: Connection parameters updated.
D NimBLERemoteService: Characteristic Discovered >> status: 14 handle: -1
D NimBLERemoteService: << Characteristic Discovered
D NimBLERemoteService: << retrieveCharacteristics()
D NimBLERemoteService: >> retrieveCharacteristics() for service: 0x0001
D NimBLERemoteService: Characteristic Discovered >> status: 0 handle: 16
D NimBLERemoteCharacteristic: >> NimBLERemoteCharacteristic()
D NimBLERemoteCharacteristic: << NimBLERemoteCharacteristic(): 0x0002
D NimBLERemoteService: Characteristic Discovered >> status: 14 handle: -1
D NimBLERemoteService: << Characteristic Discovered
D NimBLERemoteService: << retrieveCharacteristics()
D NimBLERemoteService: >> getCharacteristic: uuid: 00000003-0000-1000-8000-00805f9b34fb
D NimBLERemoteService: >> retrieveCharacteristics() for service: 0x0001
D NimBLERemoteService: Characteristic Discovered >> status: 14 handle: -1
D NimBLERemoteService: << Characteristic Discovered
D NimBLERemoteService: << retrieveCharacteristics()
D NimBLERemoteService: >> retrieveCharacteristics() for service: 0x0001
D NimBLERemoteService: Characteristic Discovered >> status: 0 handle: 18
D NimBLERemoteCharacteristic: >> NimBLERemoteCharacteristic()
D NimBLERemoteCharacteristic: << NimBLERemoteCharacteristic(): 0x0003
D NimBLERemoteService: Characteristic Discovered >> status: 14 handle: -1
D NimBLERemoteService: << Characteristic Discovered
D NimBLERemoteService: << retrieveCharacteristics()
D NimBLERemoteCharacteristic: >> setNotify(): Characteristic: uuid: 0x0003, handle: 18 0x0012, props:  0x10, 01
D NimBLERemoteCharacteristic: >> getDescriptor: uuid: 0x2902
D NimBLERemoteCharacteristic: >> retrieveDescriptors() for characteristic: 0x0003
D NimBLERemoteCharacteristic: Next Characteristic >> status: 14 handle: -1
D NimBLERemoteCharacteristic: Descriptor Discovered >> status: 0 handle: 19
D NimBLERemoteDescriptor: >> NimBLERemoteDescriptor()
D NimBLERemoteDescriptor: << NimBLERemoteDescriptor(): 0x2902
D NimBLERemoteCharacteristic: << Descriptor Discovered. status: 0
D NimBLERemoteCharacteristic: << retrieveDescriptors(): Found 1 descriptors.
D NimBLERemoteCharacteristic: << setNotify()
D NimBLERemoteDescriptor: >> Descriptor writeValue: Descriptor: uuid: 0x2902, handle: 19
Public Key: 04172730134ec3f4c85d9a38a3f69969358f303b2fe09124ad15150b88b2a790e4855aa7cadd2bab12
D NimBLERemoteCharacteristic: >> writeValue(), length: 51
I NimBLERemoteCharacteristic: Write complete; status=0 conn_handle=1
D NimBLERemoteCharacteristic: << writeValue, rc: 0
D NimBLEClient: << connect()
State changed: 3
D NimBLEClient: Got Client event 
I NimBLEClient: disconnect; reason=531, 
State changed: 24


As for folder structure, in the folder you'll find the Python Implementation (working) for the Ecoflow BLE, code is instead in the EcoflowESP32 folder. To compile use python3.10 and call platformio from it. do not use the system wide python since the version is uncompatible.
