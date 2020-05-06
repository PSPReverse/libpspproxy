from _pypspproxy import lib, ffi;

class PSPProxy(object):

    def __init__(self, sDevicePath):
        self.sDevicePath = sDevicePath;
        self.hCtx        = None;
        self.rcLibLast   = 0;

        phCtx = ffi.new("PPSPPROXYCTX");
        rcLib = lib.PSPProxyCtxCreate(phCtx, self.sDevicePath.encode("UTF-8"), ffi.NULL, ffi.NULL);
        if rcLib == 0:
            self.hCtx = phCtx[0];
        else:
            self.rcLibLast = rcLib;

    def __del__(self):
        lib.PSPProxyCtxDestroy(self.hCtx);

    def getLastRc(self):
        return self.rcLibLast;

    def setCcd(self, idCcd):
        self.rcLibLast = lib.PSPProxyCtxPspCcdSet(self.hCtx, idCcd);
        return self.rcLibLast;

    def queryLastReqRc(self):
        pVal = ffi.new("PSPSTS *");
        self.rcLibLast = lib.PSPProxyCtxQueryLastReqRc(self.hCtx, pVal);
        if self.rcLibLast == 0:
            return pVal[0];
        return self.rcLibLast;

    def readSmn(self, idCcdTgt, uSmnAddr, cbVal):
        pVal = None;
        if cbVal == 1:
            pVal = ffi.new("uint8_t *");
        elif cbVal == 2:
            pVal = ffi.new("uint16_t *");
        elif cbVal == 4:
            pVal = ffi.new("uint32_t *");
        elif cbVal == 8:
            pVal = ffi.new("uint64_t *");
        else:
            return (-1, 0);

        self.rcLibLast = lib.PSPProxyCtxPspSmnRead(self.hCtx, idCcdTgt, uSmnAddr, cbVal, pVal);
        if self.rcLibLast == 0:
            return (0, pVal[0]);
        else:
            return (self.rcLibLast, 0);

    def writeSmn(self, idCcdTgt, uSmnAddr, cbVal, uVal):
        pVal = None;
        if cbVal == 1:
            pVal = ffi.new("uint8_t *");
        elif cbVal == 2:
            pVal = ffi.new("uint16_t *");
        elif cbVal == 4:
            pVal = ffi.new("uint32_t *");
        elif cbVal == 8:
            pVal = ffi.new("uint64_t *");
        else:
            return -1;

        pVal[0] = uVal;
        self.rcLibLast = lib.PSPProxyCtxPspSmnWrite(self.hCtx, idCcdTgt, uSmnAddr, cbVal, pVal);
        if self.rcLibLast == 0:
            return 0;
        else:
            return self.rcLibLast;

    def readPspMem(self, uPspAddr, cbRead):
        abData = bytearray(cbRead);
        pvBuf = ffi.from_buffer(abData);
        self.rcLibLast = lib.PSPProxyCtxPspMemRead(self.hCtx, uPspAddr, pvBuf, cbRead);
        if self.rcLibLast == 0:
            return (0, abData);
        else:
            return (self.rcLibLast, None);

    def writePspMem(self, uPspAddr, abData):
        pvBuf = ffi.from_buffer(abData);
        self.rcLibLast = lib.PSPProxyCtxPspMemWrite(self.hCtx, uPspAddr, pvBuf, len(abData));
        return self.rcLibLast;

    def readPspMmio(self, uPspAddr, cbVal):
        pVal = None;
        if cbVal == 1:
            pVal = ffi.new("uint8_t *");
        elif cbVal == 2:
            pVal = ffi.new("uint16_t *");
        elif cbVal == 4:
            pVal = ffi.new("uint32_t *");
        elif cbVal == 8:
            pVal = ffi.new("uint64_t *");
        else:
            return (-1, 0);

        self.rcLibLast = lib.PSPProxyCtxPspMmioRead(self.hCtx, uPspAddr, cbVal, pVal);
        if self.rcLibLast == 0:
            return (0, pVal[0]);
        else:
            return (self.rcLibLast, 0);

    def writePspMmio(self, uPspAddr, cbVal, uVal):
        pVal = None;
        if cbVal == 1:
            pVal = ffi.new("uint8_t *");
        elif cbVal == 2:
            pVal = ffi.new("uint16_t *");
        elif cbVal == 4:
            pVal = ffi.new("uint32_t *");
        elif cbVal == 8:
            pVal = ffi.new("uint64_t *");
        else:
            return -1;

        pVal[0] = uVal;
        self.rcLibLast = lib.PSPProxyCtxPspMmioWrite(self.hCtx, uPspAddr, cbVal, pVal);
        if self.rcLibLast == 0:
            return 0;
        else:
            return self.rcLibLast;

    def readPspX86Mem(self, uX86Addr, cbRead):
        abData = bytearray(cbRead);
        pvBuf = ffi.from_buffer(abData);
        self.rcLibLast = lib.PSPProxyCtxPspX86MemRead(self.hCtx, uX86Addr, pvBuf, cbRead);
        if self.rcLibLast == 0:
            return (0, abData);
        else:
            return (self.rcLibLast, None);

    def writePspX86Mem(self, uX86Addr, abData):
        pvBuf = ffi.from_buffer(abData);
        self.rcLibLast = lib.PSPProxyCtxPspX86MemWrite(self.hCtx, uX86Addr, pvBuf, len(abData));
        return self.rcLibLast;

    def readPspX86Mmio(self, uX86Addr, cbVal):
        pVal = None;
        if cbVal == 1:
            pVal = ffi.new("uint8_t *");
        elif cbVal == 2:
            pVal = ffi.new("uint16_t *");
        elif cbVal == 4:
            pVal = ffi.new("uint32_t *");
        elif cbVal == 8:
            pVal = ffi.new("uint64_t *");
        else:
            return (-1, 0);

        self.rcLibLast = lib.PSPProxyCtxPspX86MmioRead(self.hCtx, uX86Addr, cbVal, pVal);
        if self.rcLibLast == 0:
            return (0, pVal[0]);
        else:
            return (self.rcLibLast, 0);

    def writePspX86Mmio(self, uX86Addr, cbVal, uVal):
        pVal = None;
        if cbVal == 1:
            pVal = ffi.new("uint8_t *");
        elif cbVal == 2:
            pVal = ffi.new("uint16_t *");
        elif cbVal == 4:
            pVal = ffi.new("uint32_t *");
        elif cbVal == 8:
            pVal = ffi.new("uint64_t *");
        else:
            return -1;

        pVal[0] = uVal;
        self.rcLibLast = lib.PSPProxyCtxPspX86MmioWrite(self.hCtx, uX86Addr, cbVal, pVal);
        if self.rcLibLast == 0:
            return 0;
        else:
            return self.rcLibLast;

    def readCoProc(self, idCoProc, idCrn, idCrm, idOpc1, idOpc2):
        pVal = ffi.new("uint32_t *");

        self.rcLibLast = lib.PSPProxyCtxPspCoProcRead(self.hCtx, idCoProc, idCrn, idCrm, idOpc1, idOpc2, pVal);
        if self.rcLibLast == 0:
            return (0, pVal[0]);
        else:
            return (self.rcLibLast, 0);

    def writeCoProc(self, idCoProc, idCrn, idCrm, idOpc1, idOpc2, uVal):
        self.rcLibLast = lib.PSPProxyCtxPspCoProcRead(self.hCtx, idCoProc, idCrn, idCrm, idOpc1, idOpc2, uVal);
        if self.rcLibLast == 0:
            return 0;
        else:
            return self.rcLibLast;

    def callSvc(self, idxSyscall, uR0, uR1, uR2, uR3):
        pR0Return = ffi.new("uint32_t *");
        self.rcLibLast = lib.PSPProxyCtxPspSvcCall(self.hCtx, idxSyscall, uR0, uR1, uR2, uR3, pR0Return);
        return (self.rcLibLast, pR0Return[0]);

    def readX86Smn(self, idNode, uSmnAddr, cbVal):
        pVal = None;
        if cbVal == 1:
            pVal = ffi.new("uint8_t *");
        elif cbVal == 2:
            pVal = ffi.new("uint16_t *");
        elif cbVal == 4:
            pVal = ffi.new("uint32_t *");
        elif cbVal == 8:
            pVal = ffi.new("uint64_t *");
        else:
            return (-1, 0);

        self.rcLibLast = lib.PSPProxyCtxX86SmnRead(self.hCtx, idNode, uSmnAddr, cbVal, pVal);
        if self.rcLibLast == 0:
            return (0, pVal[0]);
        else:
            return (self.rcLibLast, 0);

    def writeX86Smn(self, idNode, uSmnAddr, cbVal, uVal):
        pVal = None;
        if cbVal == 1:
            pVal = ffi.new("uint8_t *");
        elif cbVal == 2:
            pVal = ffi.new("uint16_t *");
        elif cbVal == 4:
            pVal = ffi.new("uint32_t *");
        elif cbVal == 8:
            pVal = ffi.new("uint64_t *");
        else:
            return -1;

        pVal[0] = uVal;
        self.rcLibLast = lib.PSPProxyCtxX86SmnWrite(self.hCtx, idNode, uSmnAddr, cbVal, pVal);
        if self.rcLibLast == 0:
            return 0;
        else:
            return self.rcLibLast;

    def allocX86Mem(self, cbMem):
        pR0KernVirtual = ffi.new("R0PTR *");
        pPhysX86Addr = ffi.new("X86PADDR *");

        self.rcLibLast = lib.PSPProxyCtxX86MemAlloc(self.hCtx, cbMem, pR0KernVirtual, pPhysX86Addr);
        if self.rcLibLast == 0:
            return (0, pR0KernVirtual[0], pPhysX86Addr[0]);
        return (self.rcLibLast, 0, 0);

    def freeX86Mem(self, R0KernVirtual):
        self.rcLibLast = lib.PSPProxyCtxX86MemFree(self.hCtx, R0KernVirtual);
        return self.rcLibLast;

    def readX86Mem(self, R0KernVirtual, cbRead):
        abData = bytearray(cbRead);
        pvBuf = ffi.from_buffer(abData);
        self.rcLibLast = lib.PSPProxyCtxX86MemRead(self.hCtx, pvBuf, R0KernVirtual, cbRead);
        if self.rcLibLast == 0:
            return (0, abData);
        else:
            return (self.rcLibLast, None);

    def writeX86Mem(self, R0KernVirtual, abData):
        pvBuf = ffi.from_buffer(abData);
        self.rcLibLast = lib.PSPProxyCtxX86MemWrite(self.hCtx, R0KernVirtual, pvBuf, len(abData));
        return self.rcLibLast;

    def readX86PhysMem(self, PhysAddr, cbRead):
        abData = bytearray(cbRead);
        pvBuf = ffi.from_buffer(abData);
        self.rcLibLast = lib.PSPProxyCtxX86PhysMemRead(self.hCtx, pvBuf, PhysAddr, cbRead);
        if self.rcLibLast == 0:
            return (0, abData);
        else:
            return (self.rcLibLast, None);

    def writeX86PhysMem(self, PhysAddr, abData):
        pvBuf = ffi.from_buffer(abData);
        self.rcLibLast = lib.PSPProxyCtxX86PhysMemWrite(self.hCtx, PhysAddr, pvBuf, len(abData));
        return self.rcLibLast;

