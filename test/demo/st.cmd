ld < ../../bin/vxWorks-68040/demoLibrary.munch
dbLoadDatabase "../../dbd/demo.dbd"
registerRecordDeviceDriver(pdbbase)
dbLoadRecords "demo.db"
iocInit
seq &demo

