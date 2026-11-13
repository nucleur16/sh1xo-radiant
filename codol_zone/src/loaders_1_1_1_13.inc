// GENERATED FILE - do not edit. Produced by codol_zone/tools/transpile_cs_loaders.py from
// CODOL-AssetExplorer src/Codol.FastFiles/Emulation/Generated/V1_1_1_13/GeneratedLoaders.cs, which is the
// machine translation ( tools/ida/transpile_loaders.py ) of the zone loaders of codoMP_client_shipRetail.exe,
// Call of Duty Online 1.1.1.13. Function names are the addresses of the original functions.

    static constexpr int TranspiledFunctionCount = 262;

    // sub_4B6C20  void()
    void F_4B6C20()
    {
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        M.WU8(M.U32(0x17C6E18U), ((byte)(((sbyte)(0)))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6E18U), ((uint)((1))));
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((1)) * 1))));
    }

    // sub_4B6C50  void __usercall(int a1@<esi>)
    void F_4B6C50(int a1)
    {
        if (!(a1 != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6AFCU), ((uint)((0))), ((uint)(a1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6AFCU), ((uint)(a1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((a1) * 1))));
        L1: ;
    }

    // sub_4B6C90  void __usercall(int a1@<eax>)
    void F_4B6C90(int a1)
    {
        int v1 = 0;
        v1 = ((int)(((uint)((4))) * ((uint)(a1))));
        if (!(((int)(((uint)((4))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6CB8U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6CB8U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4B6CD0  void __usercall(int a1@<eax>)
    void F_4B6CD0(int a1)
    {
        int v1 = 0;
        v1 = ((int)(((uint)((4))) * ((uint)(a1))));
        if (!(((int)(((uint)((4))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C693CU), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C693CU), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4B6D10  void __usercall(int a1@<eax>)
    void F_4B6D10(int a1)
    {
        int v1 = 0;
        v1 = ((int)(((uint)((4))) * ((uint)(a1))));
        if (!(((int)(((uint)((4))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x1746868U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x1746868U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4B6D50  void __usercall(int a1@<eax>)
    void F_4B6D50(int a1)
    {
        int v1 = 0;
        v1 = ((int)(((uint)((8))) * ((uint)(a1))));
        if (!(((int)(((uint)((8))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6A48U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A48U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4B6D90  void()
    void F_4B6D90()
    {
        uint v0 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6BBCU);
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((12)) * 1))));
        M.WU32(M.U32(0x17C6BBCU), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6BBCU), ((uint)((12))));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((12)) * 1))));
        L2: ;
    }

    // sub_4B6DD0  void __usercall(int a1@<eax>)
    void F_4B6DD0(int a1)
    {
        int v1 = 0;
        v1 = ((int)(((uint)((12))) * ((uint)(a1))));
        if (!(((int)(((uint)((12))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6BBCU), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6BBCU), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4B6E10  void __usercall(unsigned int result@<eax>)
    void F_4B6E10(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((2))) * result))));
        if (!(((uint)(((uint)((2))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6F8CU), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6F8CU), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4B6E50  void __usercall(unsigned int result@<eax>)
    void F_4B6E50(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((2))) * result))));
        if (!(((uint)(((uint)((2))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C69ACU), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C69ACU), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4B6E90  void()
    void F_4B6E90()
    {
        uint v0 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6B64U);
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((8)) * 1))));
        M.WU32(M.U32(0x17C6B64U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6B64U), ((uint)((8))));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((8)) * 1))));
        L2: ;
    }

    // sub_4B6ED0  void __usercall(unsigned int result@<eax>)
    void F_4B6ED0(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((2))) * result))));
        if (!(((uint)(((uint)((2))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6A90U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A90U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4B6F10  _WORD *__usercall@<eax>(int a1@<eax>, char a2)
    uint F_4B6F10(int a1, sbyte a2)
    {
        int v2 = 0;
        int v3 = 0;
        uint v4 = 0;
        int v5 = 0;
        int v6 = 0;
        uint result = 0;
        v3 = ((int)(((uint)((2))) * ((uint)(a1))));
        if (!((a2 != 0) && (v3 != 0))) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6F3CU), ((uint)((0))), ((uint)(v3)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6F3CU), ((uint)(v3)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v3) * 1))));
        L1: ;
        v4 = M.U32(0x17C6F3CU);
        if (!(a1 > (0))) goto L5;
        v5 = M.I32(0x17C6B6CU);
        v6 = a1;
        L7: ;
        M.WU16(v4, M.U16(((uint)(((int)(M.U32(((uint)(((int)(((uint)(v5)) + ((uint)((4)))))))) + ((uint)(((int)(((uint)((4))) * ((uint)(M.U16(v4)))))))))))));
        Rt::PreAdd(v4, 2);
        Rt::PreAdd(v6, -1);
        L8: ;
        if ((v6 != 0)) goto L7;
        L9: ;
        L5: ;
        result = ((uint)(v4 + (uint)((((int)(((uint)(((uint)(v3)) / 0xFFFFFFFEU))))) * 2)));
        M.WU32(0x17C6F3CU, result);
        return result;
        return {};
    }

    // sub_4B6F80  char **()
    uint F_4B6F80()
    {
        uint result = 0;
        uint v1 = 0;
        uint v2 = 0;
        result = M.U32(0x17C6B08U);
        if (!(M.U32(M.U32(0x17C6B08U)) == ((uint)((-1))))) goto L1;
        v1 = M.U32(0x1EABC3CU);
        M.WU32(M.U32(0x17C6B08U), M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v1)));
        v2 = result;
        Nat.Load_XStringData(result);
        if (!(M.U32(v2) != 0)) goto L3;
        result = Nat.SL_GetStringOfSize(M.U32(v2), ((uint)((4))), ((uint)(((int)((int)(((uint)(M.U32(v2) + (uint)((((int)(((uint)(M.StrLen(M.U32(v2)) + ((uint)((1)))))))) * 1))) - M.U32(v2)) / 1)))));
        M.WU32(v2, result);
        goto L4;
        L3: ;
        result = ((uint)((0)));
        M.WU32(v2, ((uint)((0))));
        L4: ;
        goto L2;
        L1: ;
        M.WU32(M.U32(0x17C6B08U), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(M.U32(M.U32(0x17C6B08U)) - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(M.U32(M.U32(0x17C6B08U)) - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L2: ;
        return result;
        return {};
    }

    // Load_ScriptStringArray  void __usercall(int result@<eax>)
    void F_4B7000(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        byte v4 = 0;
        v1 = result;
        v2 = ((int)(((uint)((4))) * ((uint)(result))));
        if (!(((int)(((uint)((4))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6B08U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6B08U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6B08U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = ((M.U32(v3) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6B08U, v3);
        if (!!((v4 != 0))) goto L10;
        F_4B6F80();
        L10: ;
        Rt::PreAdd(v3, 4);
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6B08U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4B7070  char **__cdecl(char a1)
    uint F_4B7070(sbyte a1)
    {
        uint result = 0;
        uint v2 = 0;
        uint v3 = 0;
        result = M.U32(0x17C6EECU);
        if (!(a1 != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        M.WU32(M.U32(0x17C6EECU), ((uint)((0))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6EECU), ((uint)((4))));
        result = M.U32(0x17C6EECU);
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((4)) * 1))));
        L1: ;
        v2 = M.U32(result);
        if (!(M.U32(result) != 0)) goto L5;
        if (!(v2 == ((uint)((-1))))) goto L7;
        v3 = M.U32(0x1EABC3CU);
        M.WU32(result, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v3)));
        return ((uint)(Nat.Load_XStringData(result)));
        goto L8;
        L7: ;
        M.WU32(result, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v2 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v2 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L8: ;
        L5: ;
        return result;
        return {};
    }

    // sub_4B70F0  int __cdecl(int a1)
    int F_4B70F0(int a1)
    {
        uint v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        int result = 0;
        v1 = M.U32(0x17C6EECU);
        if (!(a1 > (0))) goto L1;
        v2 = a1;
        L3: ;
        v3 = M.U32(v1);
        M.WU32(0x17C6EECU, v1);
        if (!(v3 != 0)) goto L6;
        if (!(v3 == ((uint)((-1))))) goto L8;
        v4 = M.U32(0x1EABC3CU);
        M.WU32(v1, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v4)));
        Nat.Load_XStringData(v1);
        goto L9;
        L8: ;
        M.WU32(v1, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v3 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v3 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L9: ;
        L6: ;
        Rt::PreAdd(v1, 4);
        Rt::PreAdd(v2, -1);
        L4: ;
        if ((v2 != 0)) goto L3;
        L5: ;
        L1: ;
        result = ((int)(((uint)((4))) * ((uint)(a1))));
        M.WU32(0x17C6EECU, ((uint)(v1 + (uint)((((int)(0 - a1))) * 4))));
        return result;
        return {};
    }

    // sub_4B7160  char *()
    uint F_4B7160()
    {
        uint v0 = 0;
        uint v1 = 0;
        byte v2 = 0;
        uint v3 = 0;
        uint result = 0;
        uint v5 = 0;
        v0 = ((uint)(M.I32(0x17C6DC4U)));
        if (!(M.U32(((uint)(M.I32(0x17C6DC4U)))) != ((uint)((-1))))) goto L1;
        v5 = ((uint)(((int)(M.U32(((uint)(M.I32(0x17C6DC4U)))) - ((uint)((1)))))));
        LBL_10: ;
        result = ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(v5 >> (28)))))))) + ((uint)(v5 & ((uint)((268435455)))))));
        M.WU32(v0, result);
        return result;
        L1: ;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v2 = ((((uint)(M.I32(0x1EBDC44U))) == ((uint)((2)))) ? (byte)1 : (byte)0);
        M.WU32(((uint)(M.I32(0x17C6DC4U))), v1);
        v0 = v1;
        M.WU32(0x1EABC3CU, v1);
        M.WU32(0x17C6EECU, v1);
        if (!(v2 != 0)) goto L3;
        M.WU32(v1, ((uint)((0))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(v1, ((uint)((4))));
        v0 = M.U32(0x17C6EECU);
        v1 = M.U32(0x1EABC3CU);
        L4: ;
        v3 = M.U32(v0);
        result = ((uint)(v1 + (uint)(((4)) * 1)));
        M.WU32(0x1EABC3CU, result);
        if (!(v3 != 0)) goto L5;
        if (!(v3 == ((uint)((-1))))) goto L7;
        M.WU32(0x1EABC3CU, result);
        M.WU32(v0, result);
        M.WI32(0x17C6910U, ((int)(result)));
        return ((uint)(Nat.Load_XStringData(v0)));
        L7: ;
        v5 = ((uint)(v3 - (uint)(((1)) * 1)));
        goto LBL_10;
        L5: ;
        return result;
        return {};
    }

    // sub_4B7280  void()
    void F_4B7280()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v0 = M.U32(0x17C6F84U);
        if (!(((uint)(M.U16(((uint)(M.U32(0x17C6F84U) + (uint)(((7)) * 2)))))) >= 0x100U)) goto L1;
        if (!(M.U32(((uint)(M.I32(0x17C6A58U)))) != 0)) goto L3;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((1))))) & 0xFFFFFFFEU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(M.I32(0x17C6A58U))), v3);
        M.WU32(0x17C69ACU, v3);
        F_4B6E50(M.U32(((uint)(v0 + (uint)(((9)) * 4)))));
        L3: ;
        goto L2;
        L1: ;
        if (!(M.U32(((uint)(M.I32(0x17C6A58U)))) != 0)) goto L5;
        v1 = M.U32(0x1EABC3CU);
        M.WU32(((uint)(M.I32(0x17C6A58U))), M.U32(0x1EABC3CU));
        v2 = ((int)(M.U32(((uint)(v0 + (uint)(((9)) * 4))))));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(0x17C6AFCU, v1);
        if (!(v2 != 0)) goto L7;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L9;
        Nat.Memset(v1, ((uint)((0))), ((uint)(v2)));
        M.WU32(0x1EABC3CU, ((uint)(v1 + (uint)((v2) * 1))));
        goto L10;
        L9: ;
        Nat.DB_LoadXFileData(v1, ((uint)(v2)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L10: ;
        L7: ;
        L5: ;
        L2: ;
    }

    // sub_4B7330  void()
    void F_4B7330()
    {
        int v0 = 0;
        if (!(((uint)(M.U16(((uint)(M.U32(0x17C6F84U) + (uint)(((7)) * 2)))))) >= 0x100U)) goto L1;
        v0 = ((int)(((uint)(((int)(((uint)((2))) * ((uint)(M.U16(((uint)(M.I32(0x17C6D14U)))))))))) + ((uint)((2)))));
        M.WU32(0x17C6A90U, M.U32(0x17C6B24U));
        if (!!((v0 != 0))) goto L3;
        return;
        L3: ;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        LBL_4: ;
        Nat.Memset(M.U32(0x17C6B24U), ((uint)((0))), ((uint)(v0)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v0) * 1))));
        return;
        L5: ;
        goto L2;
        L1: ;
        v0 = ((int)(((uint)(M.U16(((uint)(M.I32(0x17C6D14U)))))) + ((uint)((1)))));
        M.WU32(0x17C6AFCU, M.U32(0x17C6B24U));
        if (!!((v0 != 0))) goto L7;
        return;
        L7: ;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L9;
        goto LBL_4;
        L9: ;
        L2: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6B24U), ((uint)(v0)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v0) * 1))));
    }

    // sub_4B73C0  void()
    void F_4B73C0()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        int v3 = 0;
        int v4 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        M.WU32(M.U32(0x17C6E6CU), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6E6CU), ((uint)((4))));
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((4)) * 1))));
        M.WU32(0x17C6B24U, ((uint)(M.U32(0x17C6E6CU) + (uint)(((4)) * 1))));
        F_4B7330();
        if (!(M.U32(M.U32(0x17C6E6CU)) != 0)) goto L3;
        v0 = ((uint)(M.I32(0x17C6D14U)));
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(M.U32(0x17C6E6CU), v1);
        v2 = ((int)(M.U16(v0)));
        M.WU32(0x1EABC3CU, v1);
        M.WI32(0x17C6978U, ((int)(v1)));
        if (!(((ushort)(v2)) != 0)) goto L5;
        v3 = ((int)(((uint)(v2)) + ((uint)((1)))));
        goto L6;
        L5: ;
        v3 = (0);
        L6: ;
        v4 = ((int)(((uint)((4))) * ((uint)(v3))));
        if (!(((int)(((uint)((4))) * ((uint)(v3)))) != 0)) goto L7;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L9;
        Nat.Memset(v1, ((uint)((0))), ((uint)(((int)(((uint)((4))) * ((uint)(v3)))))));
        M.WU32(0x1EABC3CU, ((uint)(v1 + (uint)((v4) * 1))));
        goto L10;
        L9: ;
        Nat.DB_LoadXFileData(v1, ((uint)(((int)(((uint)((4))) * ((uint)(v3)))))));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v4) * 1))));
        L10: ;
        L7: ;
        L3: ;
    }

    // sub_4B7480  void()
    void F_4B7480()
    {
        if (!(M.U16(((uint)(M.I32(0x17C6D14U)))) != 0)) goto L1;
        M.WU32(0x17C6E6CU, M.U32(0x17C6988U));
        F_4B73C0();
        goto L2;
        L1: ;
        M.WI32(0x17C6978U, ((int)(M.U32(0x17C6988U))));
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        M.WU32(M.U32(0x17C6988U), ((uint)((0))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6988U), ((uint)((4))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((4)) * 1))));
        L2: ;
    }

    // sub_4B74E0  void()
    void F_4B74E0()
    {
        int v0 = 0;
        if (!(((uint)(M.U16(((uint)(M.U32(0x17C6F84U) + (uint)(((7)) * 2)))))) >= 0x100U)) goto L1;
        v0 = ((int)(((uint)(((int)(((uint)((2))) * ((uint)(M.U16(M.U32(0x17C68F4U)))))))) + ((uint)((2)))));
        M.WU32(0x17C6A90U, M.U32(0x17C6E0CU));
        if (!!((v0 != 0))) goto L3;
        return;
        L3: ;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        LBL_4: ;
        Nat.Memset(M.U32(0x17C6E0CU), ((uint)((0))), ((uint)(v0)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v0) * 1))));
        return;
        L5: ;
        goto L2;
        L1: ;
        v0 = ((int)(((uint)(M.U16(M.U32(0x17C68F4U)))) + ((uint)((1)))));
        M.WU32(0x17C6AFCU, M.U32(0x17C6E0CU));
        if (!!((v0 != 0))) goto L7;
        return;
        L7: ;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L9;
        goto LBL_4;
        L9: ;
        L2: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6E0CU), ((uint)(v0)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v0) * 1))));
    }

    // sub_4B7570  void()
    void F_4B7570()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        int v3 = 0;
        int v4 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        M.WU32(M.U32(0x17C6C28U), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6C28U), ((uint)((4))));
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((4)) * 1))));
        M.WU32(0x17C6E0CU, ((uint)(M.U32(0x17C6C28U) + (uint)(((4)) * 1))));
        F_4B74E0();
        if (!(M.U32(M.U32(0x17C6C28U)) != 0)) goto L3;
        v0 = M.U32(0x17C68F4U);
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(M.U32(0x17C6C28U), v1);
        v2 = ((int)(M.U16(v0)));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(0x17C6B64U, v1);
        if (!(((ushort)(v2)) != 0)) goto L5;
        v3 = ((int)(((uint)(v2)) + ((uint)((1)))));
        goto L6;
        L5: ;
        v3 = (0);
        L6: ;
        v4 = ((int)(((uint)((8))) * ((uint)(v3))));
        if (!(((int)(((uint)((8))) * ((uint)(v3)))) != 0)) goto L7;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L9;
        Nat.Memset(v1, ((uint)((0))), ((uint)(((int)(((uint)((8))) * ((uint)(v3)))))));
        M.WU32(0x1EABC3CU, ((uint)(v1 + (uint)((v4) * 1))));
        goto L10;
        L9: ;
        Nat.DB_LoadXFileData(v1, ((uint)(((int)(((uint)((8))) * ((uint)(v3)))))));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v4) * 1))));
        L10: ;
        L7: ;
        L3: ;
    }

    // sub_4B7630  void()
    void F_4B7630()
    {
        uint v0 = 0;
        byte v1 = 0;
        v0 = M.U32(0x17C68F4U);
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        M.WU32(M.U32(0x17C68F4U), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C68F4U), ((uint)((4))));
        v0 = M.U32(0x17C68F4U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((4)) * 1))));
        v1 = ((((uint)(M.U16(v0))) == ((uint)(((short)(0))))) ? (byte)1 : (byte)0);
        M.WI32(0x17C6CE4U, ((int)(((uint)(v0 + (uint)(((2)) * 2))))));
        if (!(v1 != 0)) goto L3;
        M.WU32(0x17C6B64U, ((uint)(v0 + (uint)(((2)) * 2))));
        F_4B6E90();
        goto L4;
        L3: ;
        M.WU32(0x17C6C28U, ((uint)(v0 + (uint)(((2)) * 2))));
        F_4B7570();
        L4: ;
    }

    // sub_4B7690  void __thiscall(void *this)
    void F_4B7690(uint l_this)
    {
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        byte v4 = 0;
        uint v5 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v1 = M.U32(0x17C6F58U);
        M.WU32(M.U32(0x17C6F58U), ((uint)((0))));
        M.WU32(((uint)(v1 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v1 + (uint)(((2)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6F58U), ((uint)((12))));
        v1 = M.U32(0x17C6F58U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((12)) * 1))));
        if (!(M.U32(v1) != 0)) goto L3;
        v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v2);
        M.WU32(v1, v2);
        M.WU32(0x17C69A0U, v2);
        F_4B7970(v1);
        v1 = M.U32(0x17C6F58U);
        L3: ;
        if (!(M.U32(((uint)(v1 + (uint)(((1)) * 4)))) != 0)) goto L5;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v4 = ((((uint)(M.I32(0x1EBDC44U))) == ((uint)((2)))) ? (byte)1 : (byte)0);
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(v1 + (uint)(((1)) * 4))), v3);
        M.WI32(0x17C6D14U, ((int)(v3)));
        if (!(v4 != 0)) goto L7;
        M.WU32(v3, ((uint)((0))));
        goto L8;
        L7: ;
        Nat.DB_LoadXFileData(v3, ((uint)((4))));
        v3 = M.U32(0x1EABC3CU);
        L8: ;
        M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
        M.WU32(0x17C6988U, ((uint)(((int)(((uint)(M.I32(0x17C6D14U))) + ((uint)((4))))))));
        F_4B7480();
        v1 = M.U32(0x17C6F58U);
        L5: ;
        if (!(M.U32(((uint)(v1 + (uint)(((2)) * 4)))) != 0)) goto L9;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v5);
        M.WU32(((uint)(v1 + (uint)(((2)) * 4))), v5);
        M.WU32(0x17C68F4U, v5);
        F_4B7630();
        L9: ;
    }

    // sub_4B7780  void()
    void F_4B7780()
    {
        int v0 = 0;
        if (!(((uint)(M.U16(((uint)(M.U32(0x17C6F84U) + (uint)(((7)) * 2)))))) >= 0x100U)) goto L1;
        v0 = ((int)(((uint)(((int)(((uint)((2))) * ((uint)(M.U16(M.U32(0x17C69A0U)))))))) + ((uint)((2)))));
        M.WU32(0x17C6A90U, M.U32(0x17C6D90U));
        if (!!((v0 != 0))) goto L3;
        return;
        L3: ;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        LBL_4: ;
        Nat.Memset(M.U32(0x17C6D90U), ((uint)((0))), ((uint)(v0)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v0) * 1))));
        return;
        L5: ;
        goto L2;
        L1: ;
        v0 = ((int)(((uint)(M.U16(M.U32(0x17C69A0U)))) + ((uint)((1)))));
        M.WU32(0x17C6AFCU, M.U32(0x17C6D90U));
        if (!!((v0 != 0))) goto L7;
        return;
        L7: ;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L9;
        goto LBL_4;
        L9: ;
        L2: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6D90U), ((uint)(v0)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v0) * 1))));
    }

    // sub_4B7810  void __usercall(unsigned int result@<eax>)
    void F_4B7810(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((3))) * result))));
        if (!(((uint)(((uint)((3))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6F70U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6F70U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4B7850  void __usercall(unsigned int result@<eax>)
    void F_4B7850(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((6))) * result))));
        if (!(((uint)(((uint)((6))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6B28U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6B28U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4B7890  void()
    void F_4B7890()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        v0 = M.U32(0x17C69A0U);
        if (!(M.U8(((uint)(M.U32(0x17C69A0U) + (uint)(((2)) * 1)))) != 0)) goto L1;
        if (!(M.U32(((uint)(M.I32(0x17C6E70U)))) != 0)) goto L3;
        v1 = M.U32(0x1EABC3CU);
        M.WU32(((uint)(M.I32(0x17C6E70U))), M.U32(0x1EABC3CU));
        M.WU32(0x17C6F70U, v1);
        v2 = ((int)(M.U16(v0)));
        if (!(((ushort)(v2)) != 0)) goto L5;
        F_4B7810(((uint)(((int)(((uint)(v2)) + ((uint)((1))))))));
        goto L6;
        L5: ;
        F_4B7810(((uint)((0))));
        L6: ;
        L3: ;
        goto L2;
        L1: ;
        if (!(M.U32(((uint)(M.I32(0x17C6E70U)))) != 0)) goto L7;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(M.I32(0x17C6E70U))), v3);
        M.WU32(0x17C6B28U, v3);
        v4 = ((int)(M.U16(v0)));
        if (!(((ushort)(v4)) != 0)) goto L9;
        F_4B7850(((uint)(((int)(((uint)(v4)) + ((uint)((1))))))));
        goto L10;
        L9: ;
        F_4B7850(((uint)((0))));
        L10: ;
        L7: ;
        L2: ;
    }

    // sub_4B7910  void()
    void F_4B7910()
    {
        uint v0 = 0;
        v0 = M.U32(0x17C6C14U);
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        M.WU32(M.U32(0x17C6C14U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((4)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((5)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((6)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6C14U), ((uint)((28))));
        v0 = M.U32(0x17C6C14U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((28)) * 1))));
        M.WU32(0x17C6D90U, ((uint)(v0 + (uint)(((7)) * 4))));
        F_4B7780();
        M.WI32(0x17C6E70U, ((int)(((uint)(((int)(M.U32(0x17C6C14U))))) + ((uint)((24))))));
        F_4B7890();
    }

    // sub_4B7970  void __thiscall(void *this)
    void F_4B7970(uint l_this)
    {
        uint v1 = 0;
        byte v2 = 0;
        v1 = M.U32(0x17C69A0U);
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        M.WU32(M.U32(0x17C69A0U), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C69A0U), ((uint)((4))));
        v1 = M.U32(0x17C69A0U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((4)) * 1))));
        v2 = ((((uint)(M.U16(v1))) == ((uint)(((short)(0))))) ? (byte)1 : (byte)0);
        M.WI32(0x17C69FCU, ((int)(((uint)(v1 + (uint)(((2)) * 2))))));
        if (!(v2 != 0)) goto L3;
        M.WU32(0x17C6BBCU, ((uint)(v1 + (uint)(((2)) * 2))));
        F_4B6D90();
        goto L4;
        L3: ;
        M.WU32(0x17C6C14U, ((uint)(v1 + (uint)(((2)) * 2))));
        F_4B7910();
        L4: ;
    }

    // sub_4B79D0  _WORD *__usercall@<eax>(int a1@<eax>)
    uint F_4B79D0(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        int v5 = 0;
        short v6 = 0;
        uint result = 0;
        v2 = ((int)(((uint)((8))) * ((uint)(a1))));
        if (!(((int)(((uint)((8))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6CE8U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6CE8U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6CE8U);
        if (!(a1 > (0))) goto L5;
        v4 = M.I32(0x17C6B6CU);
        v5 = a1;
        L7: ;
        v6 = ((short)(M.U16(((uint)(((int)(M.U32(((uint)(((int)(((uint)(v4)) + ((uint)((4)))))))) + ((uint)(((int)(((uint)((4))) * ((uint)(M.U16(v3))))))))))))));
        M.WU32(0x17C6F3CU, v3);
        M.WU16(v3, ((ushort)(v6)));
        v3 = ((uint)(v3 + (uint)(((4)) * 2)));
        Rt::PreAdd(v5, -1);
        L8: ;
        if ((v5 != 0)) goto L7;
        L9: ;
        L5: ;
        result = ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFEU))))) * 2)));
        M.WU32(0x17C6CE8U, result);
        return result;
        return {};
    }

    // sub_4B7A50  int()
    int F_4B7A50()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        uint v6 = 0;
        uint v7 = 0;
        uint v8 = 0;
        int v9 = 0;
        int v10 = 0;
        int v11 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6F84U);
        Nat.Memset(M.U32(0x17C6F84U), ((uint)((0))), ((uint)((88))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6F84U), ((uint)((88))));
        v0 = M.U32(0x17C6F84U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((88)) * 1))));
        F_4CEE10((3));
        v2 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v2 != 0)) goto L3;
        if (!(v2 == ((uint)((-1))))) goto L5;
        v3 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v3)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6F84U);
        goto L6;
        L5: ;
        v4 = ((uint)(v2 - (uint)(((1)) * 1)));
        v1 = ((uint)(v4 >> (28)));
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(v4 >> (28)))))))) + ((uint)(v4 & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((12)) * 4)))) != 0)) goto L7;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((1))))) & 0xFFFFFFFEU));
        M.WU32(0x1EABC3CU, v5);
        M.WU32(((uint)(v0 + (uint)(((12)) * 4))), v5);
        M.WU32(0x17C6F3CU, v5);
        F_4B6F10(((int)(M.U8(((uint)(v0 + (uint)(((26)) * 1)))))), ((sbyte)((1))));
        v0 = M.U32(0x17C6F84U);
        L7: ;
        if (!(M.U32(((uint)(v0 + (uint)(((20)) * 4)))) != 0)) goto L9;
        v6 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v6);
        M.WU32(((uint)(v0 + (uint)(((20)) * 4))), v6);
        M.WU32(0x17C6CE8U, v6);
        F_4B79D0(((int)(M.U8(((uint)(v0 + (uint)(((27)) * 1)))))));
        v0 = M.U32(0x17C6F84U);
        L9: ;
        if (!(M.U32(((uint)(v0 + (uint)(((21)) * 4)))) != 0)) goto L11;
        v7 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v7);
        M.WU32(((uint)(v0 + (uint)(((21)) * 4))), v7);
        M.WU32(0x17C6F58U, v7);
        F_4B7690(v1);
        v0 = M.U32(0x17C6F84U);
        L11: ;
        v8 = M.U32(0x1EABC3CU);
        if (!(M.U32(((uint)(v0 + (uint)(((13)) * 4)))) != 0)) goto L13;
        v9 = ((int)(M.U16(((uint)(v0 + (uint)(((2)) * 2))))));
        M.WU32(((uint)(v0 + (uint)(((13)) * 4))), M.U32(0x1EABC3CU));
        M.WU32(0x17C6AFCU, v8);
        if (!(v9 != 0)) goto L15;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L17;
        Nat.Memset(v8, ((uint)((0))), ((uint)(v9)));
        goto L18;
        L17: ;
        Nat.DB_LoadXFileData(v8, ((uint)(v9)));
        v0 = M.U32(0x17C6F84U);
        v8 = M.U32(0x1EABC3CU);
        L18: ;
        v8 = ((uint)(v8 + (uint)((v9) * 1)));
        M.WU32(0x1EABC3CU, v8);
        L15: ;
        L13: ;
        if (!(M.U32(((uint)(v0 + (uint)(((14)) * 4)))) != 0)) goto L19;
        M.WU32(0x1EABC3CU, ((uint)(((uint)(v8 + (uint)(((1)) * 1))) & 0xFFFFFFFEU)));
        M.WU32(((uint)(v0 + (uint)(((14)) * 4))), M.U32(0x1EABC3CU));
        M.WU32(0x17C6F8CU, ((uint)(((uint)(v8 + (uint)(((1)) * 1))) & 0xFFFFFFFEU)));
        F_4B6E10(((uint)(M.U16(((uint)(v0 + (uint)(((3)) * 2)))))));
        v0 = M.U32(0x17C6F84U);
        v8 = M.U32(0x1EABC3CU);
        L19: ;
        if (!(M.U32(((uint)(v0 + (uint)(((15)) * 4)))) != 0)) goto L21;
        M.WU32(0x1EABC3CU, ((uint)(((uint)(v8 + (uint)(((3)) * 1))) & 0xFFFFFFFCU)));
        M.WU32(((uint)(v0 + (uint)(((15)) * 4))), M.U32(0x1EABC3CU));
        M.WU32(0x17C6CB8U, ((uint)(((uint)(v8 + (uint)(((3)) * 1))) & 0xFFFFFFFCU)));
        F_4B6C90(((int)(M.U16(((uint)(v0 + (uint)(((4)) * 2)))))));
        v0 = M.U32(0x17C6F84U);
        v8 = M.U32(0x1EABC3CU);
        L21: ;
        if (!(M.U32(((uint)(v0 + (uint)(((16)) * 4)))) != 0)) goto L23;
        M.WU32(0x1EABC3CU, ((uint)(((uint)(v8 + (uint)(((1)) * 1))) & 0xFFFFFFFEU)));
        M.WU32(((uint)(v0 + (uint)(((16)) * 4))), M.U32(0x1EABC3CU));
        M.WU32(0x17C6F8CU, ((uint)(((uint)(v8 + (uint)(((1)) * 1))) & 0xFFFFFFFEU)));
        F_4B6E10(M.U32(((uint)(v0 + (uint)(((8)) * 4)))));
        v0 = M.U32(0x17C6F84U);
        v8 = M.U32(0x1EABC3CU);
        L23: ;
        if (!(M.U32(((uint)(v0 + (uint)(((17)) * 4)))) != 0)) goto L25;
        v10 = ((int)(M.U16(((uint)(v0 + (uint)(((5)) * 2))))));
        M.WU32(0x1EABC3CU, v8);
        M.WU32(((uint)(v0 + (uint)(((17)) * 4))), v8);
        M.WU32(0x17C6AFCU, v8);
        if (!(v10 != 0)) goto L27;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L29;
        Nat.Memset(v8, ((uint)((0))), ((uint)(v10)));
        goto L30;
        L29: ;
        Nat.DB_LoadXFileData(v8, ((uint)(v10)));
        v0 = M.U32(0x17C6F84U);
        v8 = M.U32(0x1EABC3CU);
        L30: ;
        v8 = ((uint)(v8 + (uint)((v10) * 1)));
        M.WU32(0x1EABC3CU, v8);
        L27: ;
        L25: ;
        if (!(M.U32(((uint)(v0 + (uint)(((18)) * 4)))) != 0)) goto L31;
        M.WU32(0x1EABC3CU, ((uint)(((uint)(v8 + (uint)(((3)) * 1))) & 0xFFFFFFFCU)));
        M.WU32(((uint)(v0 + (uint)(((18)) * 4))), M.U32(0x1EABC3CU));
        M.WU32(0x17C6CB8U, ((uint)(((uint)(v8 + (uint)(((3)) * 1))) & 0xFFFFFFFCU)));
        F_4B6C90(((int)(M.U16(((uint)(v0 + (uint)(((6)) * 2)))))));
        v0 = M.U32(0x17C6F84U);
        L31: ;
        M.WI32(0x17C6A58U, ((int)(((uint)(v0 + (uint)(((19)) * 4))))));
        F_4B7280();
        return F_4CEE70();
        return {};
    }

    // sub_4B7CE0  int()
    int F_4B7CE0()
    {
        int v0 = 0;
        uint v1 = 0;
        uint inserted = 0;
        int v3 = 0;
        F_4CEE10((0));
        v0 = ((int)(M.U32(((uint)(M.I32(0x17C6BF0U))))));
        if (!(M.U32(((uint)(M.I32(0x17C6BF0U)))) != 0)) goto L1;
        if (!((((uint)(v0)) == ((uint)((-1)))) || (((uint)(v0)) == ((uint)((-2)))))) goto L3;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(M.I32(0x17C6BF0U))), v1);
        M.WU32(0x17C6F84U, v1);
        if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
        inserted = F_4CEEC0();
        goto L6;
        L5: ;
        inserted = ((uint)((0)));
        L6: ;
        F_4B7A50();
        F_4CAAE0();
        if (!(inserted != 0)) goto L7;
        M.WU32(inserted, M.U32(((uint)(M.I32(0x17C6BF0U)))));
        L7: ;
        goto L4;
        L3: ;
        M.WU32(((uint)(M.I32(0x17C6BF0U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
        L4: ;
        L1: ;
        return F_4CEE70();
        return {};
    }

    // sub_4B7EA0  void __usercall(unsigned int result@<eax>)
    void F_4B7EA0(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((28))) * result))));
        if (!(((uint)(((uint)((28))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6A2CU), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A2CU), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4B7EF0  void __usercall(unsigned int result@<eax>)
    void F_4B7EF0(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((32))) * result))));
        if (!(((uint)(((uint)((32))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6924U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6924U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // Load_MssSound_data  int()
    int F_4B7F30()
    {
        uint v0 = 0;
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        uint inserted = 0;
        F_4CEE10((0));
        v0 = ((uint)(M.I32(0x17C6C3CU)));
        v1 = ((int)(M.U32(((uint)(((int)(((uint)(M.I32(0x17C6C3CU))) + ((uint)((40))))))))));
        if (!(v1 != 0)) goto L1;
        if (!((((uint)(v1)) != ((uint)((-1)))) && (((uint)(v1)) != ((uint)((-2)))))) goto L3;
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C6C3CU))) + ((uint)((40))))))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) >> (28))))))))))))));
        return F_4CEE70();
        L3: ;
        v3 = M.U32(0x1EABC3CU);
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C6C3CU))) + ((uint)((40))))))), M.U32(0x1EABC3CU));
        M.WU32(0x17C6AFCU, v3);
        if (!(((uint)(v1)) == ((uint)((-2))))) goto L5;
        inserted = F_4CEEC0();
        goto L6;
        L5: ;
        inserted = ((uint)((0)));
        L6: ;
        v4 = M.I32(((uint)(v0 + (uint)(((2)) * 4))));
        if (!(v4 != 0)) goto L7;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L9;
        Nat.Memset(v3, ((uint)((0))), ((uint)(v4)));
        goto L10;
        L9: ;
        Nat.DB_LoadXFileData(v3, ((uint)(M.I32(((uint)(v0 + (uint)(((2)) * 4)))))));
        v0 = ((uint)(M.I32(0x17C6C3CU)));
        L10: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v4) * 1))));
        L7: ;
        Nat.MSS_FixupLoadedSoundInfo(v0, ((uint)(M.I32(((uint)(v0 + (uint)(((10)) * 4)))))));
        if (!(inserted != 0)) goto L11;
        M.WU32(inserted, M.U32(((uint)(((int)(((uint)(M.I32(0x17C6C3CU))) + ((uint)((40)))))))));
        L11: ;
        L1: ;
        return F_4CEE70();
        return {};
    }

    // Load_StreamedSound  char **()
    uint F_4B8010()
    {
        uint v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint result = 0;
        uint v5 = 0;
        v0 = M.U32(0x17C6EFCU);
        v1 = ((int)(M.U32(M.U32(0x17C6EFCU))));
        M.WU32(0x17C6EECU, M.U32(0x17C6EFCU));
        if (!(v1 != 0)) goto L1;
        if (!(((uint)(v1)) == ((uint)((-1))))) goto L3;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(M.U32(0x17C6EFCU), M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6EFCU);
        goto L4;
        L3: ;
        M.WU32(M.U32(0x17C6EFCU), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L4: ;
        L1: ;
        v3 = M.U32(((uint)(v0 + (uint)(((1)) * 4))));
        result = ((uint)(v0 + (uint)(((1)) * 4)));
        M.WU32(0x17C6EECU, result);
        if (!(v3 != 0)) goto L5;
        if (!(v3 == ((uint)((-1))))) goto L7;
        v5 = M.U32(0x1EABC3CU);
        M.WU32(result, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v5)));
        return ((uint)(Nat.Load_XStringData(result)));
        goto L8;
        L7: ;
        M.WU32(result, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v3 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v3 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L8: ;
        L5: ;
        return result;
        return {};
    }

    // Load_LoadedSound  int()
    int F_4B80B0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        int v3 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6DF4U);
        Nat.Memset(M.U32(0x17C6DF4U), ((uint)((0))), ((uint)((48))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6DF4U), ((uint)((48))));
        v0 = M.U32(0x17C6DF4U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((48)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!!((v1 != 0))) goto L3;
        goto LBL_9;
        L3: ;
        if (!(v1 != ((uint)((-1))))) goto L5;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        LBL_9: ;
        M.WI32(0x17C6C3CU, ((int)(((uint)(v0 + (uint)(((1)) * 4))))));
        F_4B7F30();
        return F_4CEE70();
        L5: ;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        M.WI32(0x17C6C3CU, ((int)(((uint)(M.U32(0x17C6DF4U) + (uint)(((1)) * 4))))));
        F_4B7F30();
        return F_4CEE70();
        return {};
    }

    // Load_LoadedSoundPtr  int()
    int F_4B8170()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint inserted = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6D50U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6D50U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6D50U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6D50U))), v2);
            M.WU32(0x17C6DF4U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            inserted = F_4CEEC0();
            goto L6;
            L5: ;
            inserted = ((uint)((0)));
            L6: ;
            F_4B80B0();
            v4 = ((uint)(M.I32(0x17C6D50U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6D50U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((13))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(inserted != 0)) goto L7;
            M.WU32(inserted, M.U32(((uint)(M.I32(0x17C6D50U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // Load_SoundFile  char **__thiscall(void *this)
    uint F_4B8240(uint l_this)
    {
        uint v1 = 0;
        byte v2 = 0;
        uint v3 = 0;
        v1 = M.U32(0x17C6BA0U);
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        M.WU32(M.U32(0x17C6BA0U), ((uint)((0))));
        M.WU32(((uint)(v1 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v1 + (uint)(((2)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6BA0U), ((uint)((12))));
        v1 = M.U32(0x17C6BA0U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((12)) * 1))));
        v2 = ((((uint)(M.I8(v1))) == ((uint)(((sbyte)(1))))) ? (byte)1 : (byte)0);
        M.WI32(0x17C6BB0U, ((int)(((uint)(v1 + (uint)(((4)) * 1))))));
        if (!(v2 != 0)) goto L3;
        M.WI32(0x17C6D50U, ((int)(((uint)(v1 + (uint)(((4)) * 1))))));
        return ((uint)(F_4B8170()));
        goto L4;
        L3: ;
        M.WI32(0x17C6E84U, ((int)(((uint)(v1 + (uint)(((4)) * 1))))));
        M.WI32(0x17C6994U, ((int)(((uint)(v1 + (uint)(((4)) * 1))))));
        M.WI32(0x17C6AE4U, ((int)(((uint)(v1 + (uint)(((4)) * 1))))));
        M.WU32(0x17C6EFCU, ((uint)(v1 + (uint)(((4)) * 1))));
        return F_4B8010();
        L4: ;
        return {};
    }

    // Load_SndCurve  int()
    int F_4B82B0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        int v3 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6A08U);
        Nat.Memset(M.U32(0x17C6A08U), ((uint)((0))), ((uint)((136))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A08U), ((uint)((136))));
        v0 = M.U32(0x17C6A08U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((136)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        return F_4CEE70();
        return {};
    }

    // Load_SndCurvePtr  int()
    int F_4B8350()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint inserted = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6D70U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6D70U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6D70U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6D70U))), v2);
            M.WU32(0x17C6A08U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            inserted = F_4CEEC0();
            goto L6;
            L5: ;
            inserted = ((uint)((0)));
            L6: ;
            F_4B82B0();
            v4 = ((uint)(M.I32(0x17C6D70U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6D70U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((12))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(inserted != 0)) goto L7;
            M.WU32(inserted, M.U32(((uint)(M.I32(0x17C6D70U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // Load_SpeakerMap  char **()
    uint F_4B8420()
    {
        int v0 = 0;
        uint result = 0;
        uint v2 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        Nat.Memset(M.U32(0x17C6E48U), ((uint)((0))), ((uint)((408))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6E48U), ((uint)((408))));
        L2: ;
        v0 = ((int)(M.U32(((uint)(M.U32(0x17C6E48U) + (uint)(((1)) * 4))))));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((408)) * 1))));
        result = ((uint)(M.U32(0x17C6E48U) + (uint)(((4)) * 1)));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6E48U) + (uint)(((4)) * 1))));
        if (!(v0 != 0)) goto L3;
        if (!(((uint)(v0)) == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(result, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        return ((uint)(Nat.Load_XStringData(result)));
        goto L6;
        L5: ;
        M.WU32(result, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L6: ;
        L3: ;
        return result;
        return {};
    }

    // Load_snd_alias_t  char *()
    uint F_4B84B0()
    {
        uint v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        uint v6 = 0;
        uint v7 = 0;
        uint v8 = 0;
        uint v9 = 0;
        uint v10 = 0;
        uint v11 = 0;
        uint v12 = 0;
        uint v13 = 0;
        uint v14 = 0;
        uint v15 = 0;
        uint v16 = 0;
        uint result = 0;
        uint v18 = 0;
        v0 = M.U32(0x17C6E40U);
        v1 = ((int)(M.U32(M.U32(0x17C6E40U))));
        M.WU32(0x17C6EECU, M.U32(0x17C6E40U));
        if (!(v1 != 0)) goto L1;
        if (!(((uint)(v1)) == ((uint)((-1))))) goto L3;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(M.U32(0x17C6E40U), M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6E40U);
        goto L4;
        L3: ;
        M.WU32(M.U32(0x17C6E40U), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L4: ;
        L1: ;
        v3 = M.U32(((uint)(v0 + (uint)(((1)) * 4))));
        v4 = ((uint)(v0 + (uint)(((1)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v0 + (uint)(((1)) * 4))));
        if (!(v3 != 0)) goto L5;
        if (!(v3 == ((uint)((-1))))) goto L7;
        v5 = M.U32(0x1EABC3CU);
        M.WU32(v4, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v5)));
        Nat.Load_XStringData(v4);
        v0 = M.U32(0x17C6E40U);
        goto L8;
        L7: ;
        M.WU32(v4, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v3 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v3 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L8: ;
        L5: ;
        v6 = M.U32(((uint)(v0 + (uint)(((2)) * 4))));
        v7 = ((uint)(v0 + (uint)(((2)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v0 + (uint)(((2)) * 4))));
        if (!(v6 != 0)) goto L9;
        if (!(v6 == ((uint)((-1))))) goto L11;
        v8 = M.U32(0x1EABC3CU);
        M.WU32(v7, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v8)));
        Nat.Load_XStringData(v7);
        v0 = M.U32(0x17C6E40U);
        goto L12;
        L11: ;
        M.WU32(v7, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v6 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v6 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L12: ;
        L9: ;
        v9 = M.U32(((uint)(v0 + (uint)(((3)) * 4))));
        v10 = ((uint)(v0 + (uint)(((3)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v0 + (uint)(((3)) * 4))));
        if (!(v9 != 0)) goto L13;
        if (!(v9 == ((uint)((-1))))) goto L15;
        v11 = M.U32(0x1EABC3CU);
        M.WU32(v10, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v11)));
        Nat.Load_XStringData(v10);
        v0 = M.U32(0x17C6E40U);
        goto L16;
        L15: ;
        M.WU32(v10, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v9 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v9 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L16: ;
        L13: ;
        v12 = M.U32(((uint)(v0 + (uint)(((4)) * 4))));
        v13 = ((uint)(v0 + (uint)(((4)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v0 + (uint)(((4)) * 4))));
        if (!(v12 != 0)) goto L17;
        if (!(v12 == ((uint)((-1))))) goto L19;
        v14 = M.U32(0x1EABC3CU);
        M.WU32(v13, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v14)));
        Nat.Load_XStringData(v13);
        v0 = M.U32(0x17C6E40U);
        goto L20;
        L19: ;
        M.WU32(v13, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v12 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v12 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L20: ;
        L17: ;
        v15 = M.U32(((uint)(v0 + (uint)(((5)) * 4))));
        if (!(v15 != 0)) goto L21;
        if (!(v15 == ((uint)((-1))))) goto L23;
        v16 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v16);
        M.WU32(((uint)(v0 + (uint)(((5)) * 4))), v16);
        M.WU32(0x17C6BA0U, v16);
        F_4B8240(v0);
        v0 = M.U32(0x17C6E40U);
        goto L24;
        L23: ;
        M.WU32(((uint)(v0 + (uint)(((5)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v15 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v15 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L24: ;
        L21: ;
        M.WI32(0x17C6D70U, ((int)(((uint)(v0 + (uint)(((22)) * 4))))));
        F_4B8350();
        result = M.U32(((uint)(M.U32(0x17C6E40U) + (uint)(((26)) * 4))));
        if (!(result != 0)) goto L25;
        if (!(result == ((uint)((-1))))) goto L27;
        v18 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v18);
        M.WU32(((uint)(M.U32(0x17C6E40U) + (uint)(((26)) * 4))), v18);
        M.WU32(0x17C6E48U, v18);
        return F_4B8420();
        goto L28;
        L27: ;
        result = ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(result - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(result - (uint)(((1)) * 1))) & ((uint)((268435455)))))));
        M.WU32(((uint)(M.U32(0x17C6E40U) + (uint)(((26)) * 4))), result);
        L28: ;
        L25: ;
        return result;
        return {};
    }

    // Load_snd_alias_tArray  void __cdecl(int a1)
    void F_4B86E0(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((108))) * ((uint)(a1))));
        if (!(((int)(((uint)((108))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6E40U), ((uint)((0))), ((uint)(((int)(((uint)((108))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6E40U), ((uint)(((int)(((uint)((108))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6E40U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6E40U, v3);
        F_4B84B0();
        v3 = ((uint)(v3 + (uint)(((27)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6E40U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // Load_snd_alias_list_t  int()
    int F_4B8750()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        int v5 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6F9CU);
        M.WU32(M.U32(0x17C6F9CU), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6F9CU), ((uint)((12))));
        v0 = M.U32(0x17C6F9CU);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((12)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6F9CU);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        v3 = M.U32(((uint)(v0 + (uint)(((1)) * 4))));
        if (!(v3 != 0)) goto L7;
        if (!(v3 == ((uint)((-1))))) goto L9;
        v4 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v4);
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), v4);
        M.WU32(0x17C6E40U, v4);
        F_4B86E0(((int)(M.U32(((uint)(v0 + (uint)(((2)) * 4)))))));
        goto L10;
        L9: ;
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v3 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v3 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L10: ;
        L7: ;
        return F_4CEE70();
        return {};
    }

    // Load_snd_alias_list_ptr  int()
    int F_4B8830()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6A64U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6A64U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6A64U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6A64U))), v2);
            M.WU32(0x17C6F9CU, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4B8750();
            v4 = ((uint)(M.I32(0x17C6A64U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6A64U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((11))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6A64U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4B8920  void __usercall(int result@<eax>, char a2)
    void F_4B8920(int result, sbyte a2)
    {
        int v2 = 0;
        int v3 = 0;
        uint v4 = 0;
        byte v5 = 0;
        v2 = result;
        v3 = ((int)(((uint)((4))) * ((uint)(result))));
        if (!((a2 != 0) && (v3 != 0))) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6CDCU), ((uint)((0))), ((uint)(v3)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6CDCU), ((uint)(v3)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v3) * 1))));
        L1: ;
        v4 = M.U32(0x17C6CDCU);
        if (!(v2 > (0))) goto L5;
        L7: ;
        v5 = ((((uint)(M.I32(v4))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, v4);
        if (!!((v5 != 0))) goto L10;
        M.WI32(0x17C6DC4U, ((int)(v4)));
        F_4B7160();
        M.WI32(v4, ((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))));
        L10: ;
        Rt::PreAdd(v4, 4);
        Rt::PreAdd(v2, -1);
        L8: ;
        if ((v2 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6CDCU, ((uint)(v4 + (uint)((((int)(((uint)(((uint)(v3)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4B8CD0  void __usercall(unsigned int result@<eax>)
    void F_4B8CD0(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((32))) * result))));
        if (!(((uint)(((uint)((32))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6EC0U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6EC0U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4B8D10  void __usercall(unsigned int result@<eax>)
    void F_4B8D10(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((60))) * result))));
        if (!(((uint)(((uint)((60))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6C0CU), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6C0CU), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4B8D50  void()
    void F_4B8D50()
    {
        uint v0 = 0;
        uint v1 = 0;
        byte v2 = 0;
        int v3 = 0;
        uint v4 = 0;
        int v5 = 0;
        int v6 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6D18U);
        M.WU32(M.U32(0x17C6D18U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((4)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((5)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((6)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((7)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((8)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((9)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6D18U), ((uint)((40))));
        v0 = M.U32(0x17C6D18U);
        L2: ;
        v1 = ((uint)(M.U32(0x1EABC3CU) + (uint)(((40)) * 1)));
        v2 = ((M.U32(((uint)(v0 + (uint)(((7)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((40)) * 1))));
        if (!!((v2 != 0))) goto L3;
        v1 = ((uint)(((uint)(v1 + (uint)(((15)) * 1))) & 0xFFFFFFF0U));
        v3 = ((int)(((uint)((16))) * M.U32(((uint)(v0 + (uint)(((6)) * 4))))));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(v0 + (uint)(((7)) * 4))), v1);
        M.WI32(0x17C6A10U, ((int)(v1)));
        if (!(v3 != 0)) goto L5;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L7;
        Nat.Memset(v1, ((uint)((0))), ((uint)(v3)));
        goto L8;
        L7: ;
        Nat.DB_LoadXFileData(v1, ((uint)(v3)));
        v0 = M.U32(0x17C6D18U);
        v1 = M.U32(0x1EABC3CU);
        L8: ;
        v1 = ((uint)(v1 + (uint)((v3) * 1)));
        M.WU32(0x1EABC3CU, v1);
        L5: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((9)) * 4)))) != 0)) goto L9;
        v4 = ((uint)(((uint)(v1 + (uint)(((1)) * 1))) & 0xFFFFFFFEU));
        M.WU32(((uint)(v0 + (uint)(((9)) * 4))), v4);
        v5 = ((int)(M.U32(((uint)(v0 + (uint)(((8)) * 4))))));
        v2 = ((((uint)(((int)(((uint)((2))) * ((uint)(v5)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v6 = ((int)(((uint)((2))) * ((uint)(v5))));
        M.WU32(0x1EABC3CU, v4);
        M.WI32(0x17C6C18U, ((int)(v4)));
        if (!!((v2 != 0))) goto L11;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L13;
        Nat.Memset(v4, ((uint)((0))), ((uint)(v6)));
        M.WU32(0x1EABC3CU, ((uint)(v4 + (uint)((v6) * 1))));
        goto L14;
        L13: ;
        Nat.DB_LoadXFileData(v4, ((uint)(v6)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v6) * 1))));
        L14: ;
        L11: ;
        L9: ;
    }

    // sub_4B8E60  void __usercall(int result@<eax>)
    void F_4B8E60(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        uint v5 = 0;
        v1 = result;
        v2 = ((int)(((uint)((12))) * ((uint)(result))));
        if (!(((int)(((uint)((12))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6B44U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6B44U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6B44U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = ((int)(M.U32(((uint)(v3 + (uint)(((2)) * 4))))));
        M.WU32(0x17C6B44U, v3);
        if (!(v4 != 0)) goto L10;
        if (!(((uint)(v4)) == ((uint)((-1))))) goto L12;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v5);
        M.WU32(((uint)(v3 + (uint)(((2)) * 4))), v5);
        M.WU32(0x17C6D18U, v5);
        F_4B8D50();
        goto L13;
        L12: ;
        M.WU32(((uint)(v3 + (uint)(((2)) * 4))), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v4)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v4)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L13: ;
        L10: ;
        v3 = ((uint)(v3 + (uint)(((3)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6B44U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4B8F40  void()
    void F_4B8F40()
    {
        int v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        int v3 = 0;
        uint v4 = 0;
        int v5 = 0;
        v1 = ((uint)(M.I32(0x17C6C94U)));
        v2 = ((int)(M.U32(((uint)(((int)(((uint)(M.I32(0x17C6C94U))) + ((uint)((8))))))))));
        if (!(v2 != 0)) goto L1;
        if (!(((uint)(v2)) == ((uint)((-1))))) goto L3;
        v3 = ((int)(M.I16(((uint)(((int)(((uint)(M.I32(0x17C6C94U))) + ((uint)((6))))))))));
        v4 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((1))))) & 0xFFFFFFFEU));
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C6C94U))) + ((uint)((8))))))), v4);
        M.WU32(0x1EABC3CU, v4);
        M.WU32(0x17C6B8CU, v4);
        v5 = ((int)(((uint)(M.I16(v1))) + ((uint)(((int)(((uint)(((int)(((uint)((3))) * ((uint)(M.I16(((uint)(v1 + (uint)(((1)) * 2)))))))))) + ((uint)(((int)(((uint)(((int)(((uint)((5))) * ((uint)(M.I16(((uint)(v1 + (uint)(((2)) * 2)))))))))) + ((uint)(((int)(((uint)((7))) * ((uint)(v3))))))))))))))));
        v0 = ((int)(((uint)((2))) * ((uint)(v5))));
        if (!(((int)(((uint)((2))) * ((uint)(v5)))) != 0)) goto L5;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L7;
        Nat.Memset(M.U32(0x17C6B8CU), ((uint)((0))), ((uint)(v0)));
        goto L8;
        L7: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6B8CU), ((uint)(v0)));
        L8: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v0) * 1))));
        L5: ;
        goto L4;
        L3: ;
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C6C94U))) + ((uint)((8))))))), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v2)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v2)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L4: ;
        L1: ;
    }

    // sub_4B8FB0  void __usercall(unsigned int result@<eax>)
    void F_4B8FB0(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((2))) * result))));
        if (!(((uint)(((uint)((2))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6914U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6914U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4B8FF0  int()
    int F_4B8FF0()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        int v5 = 0;
        uint v6 = 0;
        int v7 = 0;
        uint v8 = 0;
        int v9 = 0;
        int v10 = 0;
        int v11 = 0;
        int v12 = 0;
        int v13 = 0;
        int v14 = 0;
        int v15 = 0;
        v0 = ((uint)(M.U32(0x17C6D1CU) + (uint)(((20)) * 1)));
        M.WI32(0x17C6A6CU, ((int)(((uint)(((int)(M.U32(0x17C6D1CU))))) + ((uint)((3))))));
        M.WU32(0x17C6AFCU, ((uint)(M.U32(0x17C6D1CU) + (uint)(((3)) * 1))));
        M.WU8(((uint)(M.U32(0x17C6D1CU) + (uint)(((3)) * 1))), ((byte)(M.I32(0x18FD9D4U))));
        M.WI32(0x17C6C94U, ((int)(v0)));
        F_4B8F40();
        F_4CEE10((6));
        v1 = M.U32(0x17C6D1CU);
        v2 = ((int)(M.U32(((uint)(M.U32(0x17C6D1CU) + (uint)(((8)) * 4))))));
        if (!(v2 != 0)) goto L1;
        if (!(((uint)(v2)) == ((uint)((-1))))) goto L3;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((15))))) & 0xFFFFFFF0U));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(M.U32(0x17C6D1CU) + (uint)(((8)) * 4))), v3);
        M.WU32(0x17C6EC0U, v3);
        F_4B8CD0(((uint)(M.U16(((uint)(v1 + (uint)(((2)) * 2)))))));
        v1 = M.U32(0x17C6D1CU);
        goto L4;
        L3: ;
        M.WU32(((uint)(M.U32(0x17C6D1CU) + (uint)(((8)) * 4))), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v2)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v2)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L4: ;
        L1: ;
        F_4CEE70();
        v14 = ((int)(((uint)((32))) * ((uint)(M.U16(((uint)(v1 + (uint)(((2)) * 2))))))));
        v12 = ((int)(M.U32(((uint)(v1 + (uint)(((8)) * 4))))));
        v10 = ((int)(((uint)(M.U8(((uint)(v1 + (uint)(((2)) * 1)))))) & ((uint)((32)))));
        M.WI32(0x17C6F28U, ((int)(((uint)(v1 + (uint)(((9)) * 4))))));
        Nat.Stub(0xA916D0U);
        v4 = M.U32(0x17C6D1CU);
        v5 = ((int)(M.U32(((uint)(M.U32(0x17C6D1CU) + (uint)(((11)) * 4))))));
        if (!(v5 != 0)) goto L5;
        if (!(((uint)(v5)) == ((uint)((-1))))) goto L7;
        v6 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v6);
        M.WU32(((uint)(M.U32(0x17C6D1CU) + (uint)(((11)) * 4))), v6);
        M.WU32(0x17C6B44U, v6);
        F_4B8E60(((int)(M.U32(((uint)(v4 + (uint)(((10)) * 4)))))));
        v4 = M.U32(0x17C6D1CU);
        goto L8;
        L7: ;
        M.WU32(((uint)(M.U32(0x17C6D1CU) + (uint)(((11)) * 4))), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v5)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v5)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L8: ;
        L5: ;
        F_4CEE10((7));
        v7 = ((int)(M.U32(((uint)(v4 + (uint)(((4)) * 4))))));
        if (!(v7 != 0)) goto L9;
        if (!(((uint)(v7)) == ((uint)((-1))))) goto L11;
        v8 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((15))))) & 0xFFFFFFF0U));
        M.WU32(0x1EABC3CU, v8);
        M.WU32(((uint)(v4 + (uint)(((4)) * 4))), v8);
        M.WU32(0x17C6914U, v8);
        F_4B8FB0(((uint)(((int)(((uint)((3))) * ((uint)(M.U16(((uint)(v4 + (uint)(((3)) * 2)))))))))));
        v4 = M.U32(0x17C6D1CU);
        goto L12;
        L11: ;
        M.WU32(((uint)(v4 + (uint)(((4)) * 4))), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v7)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v7)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L12: ;
        L9: ;
        F_4CEE70();
        v15 = ((int)(((uint)((3))) * ((uint)(M.U16(((uint)(v4 + (uint)(((3)) * 2))))))));
        v13 = ((int)(M.U32(((uint)(v4 + (uint)(((4)) * 4))))));
        v11 = ((int)(((uint)(M.U8(((uint)(v4 + (uint)(((2)) * 1)))))) & ((uint)((32)))));
        M.WI32(0x17C6E4CU, ((int)(((uint)(v4 + (uint)(((12)) * 4))))));
        return ((int)(Nat.Stub(0xA91670U)));
        return {};
    }

    // sub_4B9180  void __cdecl(int a1)
    void F_4B9180(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((84))) * ((uint)(a1))));
        if (!(((int)(((uint)((84))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6D1CU), ((uint)((0))), ((uint)(((int)(((uint)((84))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6D1CU), ((uint)(((int)(((uint)((84))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6D1CU);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6D1CU, v3);
        F_4B8FF0();
        v3 = ((uint)(v3 + (uint)(((84)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6D1CU, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4B91F0  int()
    int F_4B91F0()
    {
        int result = 0;
        uint v1 = 0;
        result = ((int)(M.U32(((uint)(M.I32(0x17C6E28U))))));
        if (!(M.U32(((uint)(M.I32(0x17C6E28U)))) != 0)) goto L1;
        if (!(((uint)(result)) == ((uint)((-1))))) goto L3;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(M.I32(0x17C6E28U))), v1);
        M.WU32(0x17C6B90U, v1);
        F_4B9250();
        return ((int)(Nat.Stub(0xA927F0U)));
        goto L4;
        L3: ;
        result = ((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(result)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(result)) - ((uint)((1))))))) & ((uint)((268435455)))))))));
        M.WU32(((uint)(M.I32(0x17C6E28U))), ((uint)(result)));
        L4: ;
        L1: ;
        return result;
        return {};
    }

    // sub_4B9250  void()
    void F_4B9250()
    {
        uint v0 = 0;
        int v1 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6B90U);
        M.WU32(M.U32(0x17C6B90U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6B90U), ((uint)((16))));
        v0 = M.U32(0x17C6B90U);
        L2: ;
        v1 = ((int)(M.U32(((uint)(v0 + (uint)(((3)) * 4))))));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((16)) * 1))));
        M.WU32(0x17C6AFCU, ((uint)(v0 + (uint)(((4)) * 4))));
        if (!(v1 != 0)) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(((uint)(v0 + (uint)(((4)) * 4))), ((uint)((0))), ((uint)(v1)));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(((uint)(v0 + (uint)(((4)) * 4))), ((uint)(v1)));
        L6: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L3: ;
    }

    // sub_4B92D0  int()
    int F_4B92D0()
    {
        uint v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6BD4U);
        M.WU32(M.U32(0x17C6BD4U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((4)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((5)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((6)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((7)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((8)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6BD4U), ((uint)((36))));
        v0 = M.U32(0x17C6BD4U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((36)) * 1))));
        F_4CEE10((3));
        v1 = ((int)(M.U32(((uint)(v0 + (uint)(((8)) * 4))))));
        v2 = ((uint)(v0 + (uint)(((32)) * 1)));
        M.WU32(0x17C6EECU, ((uint)(v0 + (uint)(((32)) * 1))));
        if (!(v1 != 0)) goto L3;
        if (!(((uint)(v1)) == ((uint)((-1))))) goto L5;
        v3 = M.U32(0x1EABC3CU);
        M.WU32(v2, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v3)));
        Nat.Load_XStringData(v2);
        M.WI32(0x17C6E28U, ((int)(M.U32(0x17C6BD4U))));
        F_4B91F0();
        return F_4CEE70();
        L5: ;
        M.WU32(v2, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L3: ;
        M.WI32(0x17C6E28U, ((int)(v0)));
        F_4B91F0();
        return F_4CEE70();
        return {};
    }

    // sub_4B93A0  int()
    int F_4B93A0()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint inserted = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(M.U32(0x17C6F98U))));
            if (!(M.U32(M.U32(0x17C6F98U)) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(M.U32(0x17C6F98U), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(M.U32(0x17C6F98U), v2);
            M.WU32(0x17C6BD4U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            inserted = F_4CEEC0();
            goto L6;
            L5: ;
            inserted = ((uint)((0)));
            L6: ;
            F_4B92D0();
            v4 = M.U32(0x17C6F98U);
            M.WI32((_sp + 0x0U), ((int)(M.U32(M.U32(0x17C6F98U)))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((10))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(inserted != 0)) goto L7;
            M.WU32(inserted, M.U32(M.U32(0x17C6F98U)));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4B9470  void __usercall(int result@<eax>)
    void F_4B9470(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = result;
        v2 = ((int)(((uint)((4))) * ((uint)(result))));
        if (!(((int)(((uint)((4))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6F98U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6F98U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6F98U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6F98U, v3);
        F_4B93A0();
        v3 = ((uint)(v3 + (uint)(((4)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6F98U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4B95A0  int()
    int F_4B95A0()
    {
        uint v0 = 0;
        uint v1 = 0;
        byte v2 = 0;
        int v3 = 0;
        uint v4 = 0;
        int v5 = 0;
        int v6 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6C68U);
        Nat.Memset(M.U32(0x17C6C68U), ((uint)((0))), ((uint)((68))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6C68U), ((uint)((68))));
        v0 = M.U32(0x17C6C68U);
        L2: ;
        v1 = ((uint)(M.U32(0x1EABC3CU) + (uint)(((68)) * 1)));
        v2 = ((M.U32(((uint)(v0 + (uint)(((1)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((68)) * 1))));
        if (!!((v2 != 0))) goto L3;
        v1 = ((uint)(((uint)(v1 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v2 = ((((uint)(((int)(((uint)((8))) * ((uint)(((int)(M.U32(((uint)(v0 + (uint)(((3)) * 4)))) * M.U32(((uint)(v0 + (uint)(((4)) * 4)))))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v3 = ((int)(((uint)((8))) * ((uint)(((int)(M.U32(((uint)(v0 + (uint)(((3)) * 4)))) * M.U32(((uint)(v0 + (uint)(((4)) * 4))))))))));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), v1);
        M.WI32(0x17C6DD4U, ((int)(v1)));
        if (!!((v2 != 0))) goto L5;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L7;
        Nat.Memset(v1, ((uint)((0))), ((uint)(v3)));
        goto L8;
        L7: ;
        Nat.DB_LoadXFileData(v1, ((uint)(v3)));
        v0 = M.U32(0x17C6C68U);
        v1 = M.U32(0x1EABC3CU);
        L8: ;
        v1 = ((uint)(v1 + (uint)((v3) * 1)));
        M.WU32(0x1EABC3CU, v1);
        L5: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((2)) * 4)))) != 0)) goto L9;
        v4 = ((uint)(((uint)(v1 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v2 = ((((uint)(((int)(((uint)((4))) * ((uint)(((int)(M.U32(((uint)(v0 + (uint)(((3)) * 4)))) * M.U32(((uint)(v0 + (uint)(((4)) * 4)))))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v5 = ((int)(((uint)((4))) * ((uint)(((int)(M.U32(((uint)(v0 + (uint)(((3)) * 4)))) * M.U32(((uint)(v0 + (uint)(((4)) * 4))))))))));
        M.WU32(0x1EABC3CU, v4);
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), v4);
        M.WU32(0x1746868U, v4);
        if (!!((v2 != 0))) goto L11;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L13;
        Nat.Memset(v4, ((uint)((0))), ((uint)(v5)));
        goto L14;
        L13: ;
        Nat.DB_LoadXFileData(v4, ((uint)(v5)));
        v0 = M.U32(0x17C6C68U);
        v4 = M.U32(0x1EABC3CU);
        L14: ;
        M.WU32(0x1EABC3CU, ((uint)(v4 + (uint)((v5) * 1))));
        L11: ;
        L9: ;
        M.WU32(0x17C6F98U, ((uint)(v0 + (uint)(((16)) * 4))));
        return F_4B93A0();
        return {};
    }

    // sub_4B9700  void()
    void F_4B9700()
    {
        uint v0 = 0;
        byte v1 = 0;
        int v2 = 0;
        if (!(M.U32(((uint)(M.I32(0x17C6D2CU)))) != 0)) goto L1;
        v0 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v1 = ((((uint)(((int)(((uint)((4))) * ((uint)(M.U16(((uint)(((int)(((uint)(M.I32(0x17C6D2CU))) + ((uint)((4)))))))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v2 = ((int)(((uint)((4))) * ((uint)(M.U16(((uint)(((int)(((uint)(M.I32(0x17C6D2CU))) + ((uint)((4))))))))))));
        M.WU32(0x1EABC3CU, v0);
        M.WU32(((uint)(M.I32(0x17C6D2CU))), v0);
        M.WI32(0x17C6C88U, ((int)(v0)));
        if (!!((v1 != 0))) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(v0, ((uint)((0))), ((uint)(v2)));
        M.WU32(0x1EABC3CU, ((uint)(v0 + (uint)((v2) * 1))));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v0, ((uint)(v2)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L6: ;
        L3: ;
        L1: ;
    }

    // sub_4B9770  void()
    void F_4B9770()
    {
        uint v0 = 0;
        byte v1 = 0;
        int v2 = 0;
        if (!(M.U32(((uint)(M.I32(0x17C6D88U)))) != 0)) goto L1;
        v0 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v1 = ((((uint)(((int)(((uint)((4))) * ((uint)(M.U16(((uint)(((int)(((uint)(M.I32(0x17C6D88U))) + ((uint)((4)))))))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v2 = ((int)(((uint)((4))) * ((uint)(M.U16(((uint)(((int)(((uint)(M.I32(0x17C6D88U))) + ((uint)((4))))))))))));
        M.WU32(0x1EABC3CU, v0);
        M.WU32(((uint)(M.I32(0x17C6D88U))), v0);
        M.WI32(0x17C6C88U, ((int)(v0)));
        if (!!((v1 != 0))) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(v0, ((uint)((0))), ((uint)(v2)));
        M.WU32(0x1EABC3CU, ((uint)(v0 + (uint)((v2) * 1))));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v0, ((uint)(v2)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L6: ;
        L3: ;
        L1: ;
    }

    // sub_4B97E0  int()
    int F_4B97E0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        int v3 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6EE4U);
        M.WU32(M.U32(0x17C6EE4U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6EE4U), ((uint)((16))));
        v0 = M.U32(0x17C6EE4U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((16)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6EE4U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        M.WI32(0x17C6DACU, ((int)(((uint)(v0 + (uint)(((1)) * 4))))));
        M.WI32(0x17C6D2CU, ((int)(((uint)(v0 + (uint)(((2)) * 4))))));
        F_4B9700();
        Nat.Stub(0xA77F20U);
        return F_4CEE70();
        return {};
    }

    // sub_4B98A0  int()
    int F_4B98A0()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint inserted = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6D8CU))))));
            if (!(M.U32(((uint)(M.I32(0x17C6D8CU)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6D8CU))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6D8CU))), v2);
            M.WU32(0x17C6EE4U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            inserted = F_4CEEC0();
            goto L6;
            L5: ;
            inserted = ((uint)((0)));
            L6: ;
            F_4B97E0();
            v4 = ((uint)(M.I32(0x17C6D8CU)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6D8CU)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((7))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(inserted != 0)) goto L7;
            M.WU32(inserted, M.U32(((uint)(M.I32(0x17C6D8CU)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4B9970  int()
    int F_4B9970()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        int v3 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6CB4U);
        M.WU32(M.U32(0x17C6CB4U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6CB4U), ((uint)((16))));
        v0 = M.U32(0x17C6CB4U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((16)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6CB4U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        M.WI32(0x17C6904U, ((int)(((uint)(v0 + (uint)(((1)) * 4))))));
        M.WI32(0x17C6D88U, ((int)(((uint)(v0 + (uint)(((2)) * 4))))));
        F_4B9770();
        Nat.Stub(0xA77EC0U);
        return F_4CEE70();
        return {};
    }

    // sub_4B9A30  int()
    int F_4B9A30()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint inserted = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6B80U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6B80U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6B80U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6B80U))), v2);
            M.WU32(0x17C6CB4U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            inserted = F_4CEEC0();
            goto L6;
            L5: ;
            inserted = ((uint)((0)));
            L6: ;
            F_4B9970();
            v4 = ((uint)(M.I32(0x17C6B80U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6B80U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((6))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(inserted != 0)) goto L7;
            M.WU32(inserted, M.U32(((uint)(M.I32(0x17C6B80U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4B9B00  int()
    int F_4B9B00()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        int v3 = 0;
        int v4 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6D80U);
        Nat.Memset(M.U32(0x17C6D80U), ((uint)((0))), ((uint)((100))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6D80U), ((uint)((100))));
        v0 = M.U32(0x17C6D80U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((100)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6D80U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        v4 = ((int)(M.U8(((uint)(v0 + (uint)(((4)) * 1))))));
        M.WI32(0x17C6A50U, ((int)(((uint)(v0 + (uint)(((2)) * 4))))));
        Nat.Stub(0xA91F60U);
        return F_4CEE70();
        return {};
    }

    // sub_4B9BB0  int()
    int F_4B9BB0()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6A5CU))))));
            if (!(M.U32(((uint)(M.I32(0x17C6A5CU)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6A5CU))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6A5CU))), v2);
            M.WU32(0x17C6D80U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4B9B00();
            v4 = ((uint)(M.I32(0x17C6A5CU)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6A5CU)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((8))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6A5CU)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4B9CA0  char *__usercall@<eax>(int a1@<eax>)
    uint F_4B9CA0(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint result = 0;
        uint v4 = 0;
        int v5 = 0;
        uint v6 = 0;
        uint v7 = 0;
        uint v8 = 0;
        byte v9 = 0;
        int v10 = 0;
        int v11 = 0;
        v2 = ((int)(((uint)((8))) * ((uint)(a1))));
        v10 = ((int)(((uint)((8))) * ((uint)(a1))));
        if (!(((int)(((uint)((8))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6A74U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A74U), ((uint)(v2)));
        L4: ;
        result = ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        goto L2;
        L1: ;
        result = M.U32(0x1EABC3CU);
        L2: ;
        v4 = M.U32(0x17C6A74U);
        if (!(a1 > (0))) goto L5;
        v11 = a1;
        L7: ;
        v5 = ((int)(M.U16(v4)));
        v6 = ((uint)(v4 + (uint)(((1)) * 4)));
        M.WU32(0x17C6A74U, v4);
        M.WI32(0x17C6A94U, ((int)(((uint)(v4 + (uint)(((1)) * 4))))));
        if (!((((uint)(v5)) == ((uint)((1)))) || (((uint)(v5)) == ((uint)((7)))))) goto L10;
        v7 = M.U32(v6);
        if (!(M.U32(v6) != 0)) goto L12;
        if (!(v7 == ((uint)((-1))))) goto L14;
        v8 = ((uint)(((uint)(result + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v9 = ((((uint)(M.I32(0x1EBDC44U))) == ((uint)((2)))) ? (byte)1 : (byte)0);
        M.WU32(0x1EABC3CU, v8);
        M.WU32(v6, v8);
        M.WU32(0x1746868U, v8);
        if (!(v9 != 0)) goto L16;
        M.WU32(v8, ((uint)((0))));
        M.WU32(((uint)(v8 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v8 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v8 + (uint)(((3)) * 4))), ((uint)((0))));
        result = ((uint)(v8 + (uint)(((16)) * 1)));
        M.WU32(0x1EABC3CU, result);
        goto L17;
        L16: ;
        Nat.DB_LoadXFileData(v8, ((uint)((16))));
        result = ((uint)(M.U32(0x1EABC3CU) + (uint)(((16)) * 1)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((16)) * 1))));
        L17: ;
        goto L15;
        L14: ;
        v2 = v10;
        M.WU32(v6, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v7 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v7 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L15: ;
        L12: ;
        L10: ;
        v4 = ((uint)(v4 + (uint)(((2)) * 4)));
        Rt::PreAdd(v11, -1);
        L8: ;
        if ((v11 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6A74U, ((uint)(v4 - (uint)((v2) * 1))));
        return result;
        return {};
    }

    // sub_4B9DC0  void __usercall(unsigned int result@<eax>)
    void F_4B9DC0(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((8))) * result))));
        if (!(((uint)(((uint)((8))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6EA4U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6EA4U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4B9E00  char *()
    uint F_4B9E00()
    {
        uint result = 0;
        uint v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        M.WI32(0x17C6A5CU, ((int)(M.U32(0x17C69E8U))));
        F_4B9BB0();
        M.WI32(0x17C6D8CU, ((int)(((uint)(((int)(M.U32(0x17C69E8U))))) + ((uint)((4))))));
        F_4B98A0();
        M.WI32(0x17C6B80U, ((int)(((uint)(((int)(M.U32(0x17C69E8U))))) + ((uint)((8))))));
        result = ((uint)(F_4B9A30()));
        v1 = M.U32(0x17C69E8U);
        if (!(M.U32(((uint)(M.U32(0x17C69E8U) + (uint)(((4)) * 4)))) != 0)) goto L1;
        v2 = ((int)(M.U8(((uint)(M.U32(0x17C69E8U) + (uint)(((13)) * 1))))));
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(M.U32(0x17C69E8U) + (uint)(((4)) * 4))), v3);
        M.WU32(0x1EABC3CU, v3);
        M.WU32(0x17C6A74U, v3);
        return F_4B9CA0(((int)(((uint)(M.U8(((uint)(v1 + (uint)(((12)) * 1)))))) + ((uint)(((int)(((uint)(v2)) + ((uint)(M.U8(((uint)(v1 + (uint)(((14)) * 1)))))))))))));
        L1: ;
        return result;
        return {};
    }

    // sub_4B9E80  void __cdecl(int a1)
    void F_4B9E80(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((20))) * ((uint)(a1))));
        if (!(((int)(((uint)((20))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C69E8U), ((uint)((0))), ((uint)(((int)(((uint)((20))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C69E8U), ((uint)(((int)(((uint)((20))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C69E8U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C69E8U, v3);
        F_4B9E00();
        v3 = ((uint)(v3 + (uint)(((20)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C69E8U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4B9EF0  char **()
    uint F_4B9EF0()
    {
        uint v0 = 0;
        int v1 = 0;
        uint result = 0;
        int v3 = 0;
        uint v4 = 0;
        v0 = M.U32(0x17C696CU);
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        M.WU32(M.U32(0x17C696CU), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C696CU), ((uint)((8))));
        v0 = M.U32(0x17C696CU);
        L2: ;
        v1 = ((int)(M.U16(((uint)(v0 + (uint)(((3)) * 2))))));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((8)) * 1))));
        M.WU32(0x17C69E8U, ((uint)(v0 + (uint)(((2)) * 4))));
        F_4B9E80(v1);
        result = M.U32(0x17C696CU);
        v3 = ((int)(M.U32(M.U32(0x17C696CU))));
        M.WU32(0x17C6EECU, M.U32(0x17C696CU));
        if (!(v3 != 0)) goto L3;
        if (!(((uint)(v3)) == ((uint)((-1))))) goto L5;
        v4 = M.U32(0x1EABC3CU);
        M.WU32(M.U32(0x17C696CU), M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v4)));
        return ((uint)(Nat.Load_XStringData(result)));
        goto L6;
        L5: ;
        M.WU32(M.U32(0x17C696CU), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v3)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v3)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L6: ;
        L3: ;
        return result;
        return {};
    }

    // sub_4B9F80  int()
    int F_4B9F80()
    {
        int result = 0;
        uint v1 = 0;
        if (!(((uint)(M.U8(((uint)(M.U32(0x17C6A88U) + (uint)(((7)) * 1)))))) == ((uint)(((sbyte)(11)))))) goto L1;
        result = ((int)(M.U32(((uint)(M.I32(0x17C6BD8U))))));
        if (!(M.U32(((uint)(M.I32(0x17C6BD8U)))) != 0)) goto L3;
        if (!(((uint)(result)) == ((uint)((-1))))) goto L5;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(M.I32(0x17C6BD8U))), v1);
        M.WU32(0x17C6C68U, v1);
        F_4B95A0();
        return ((int)(Nat.Stub(0xA91380U)));
        goto L6;
        L5: ;
        result = ((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(result)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(result)) - ((uint)((1))))))) & ((uint)((268435455)))))))));
        M.WU32(((uint)(M.I32(0x17C6BD8U))), ((uint)(result)));
        L6: ;
        L3: ;
        goto L2;
        L1: ;
        M.WU32(0x17C6F98U, ((uint)(M.I32(0x17C6BD8U))));
        return F_4B93A0();
        L2: ;
        return result;
        return {};
    }

    // sub_4BA000  void __cdecl(int a1)
    void F_4BA000(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((12))) * ((uint)(a1))));
        if (!(((int)(((uint)((12))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6A88U), ((uint)((0))), ((uint)(((int)(((uint)((12))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A88U), ((uint)(((int)(((uint)((12))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6A88U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6A88U, v3);
        M.WI32(0x17C6BD8U, ((int)(((uint)(v3 + (uint)(((8)) * 1))))));
        F_4B9F80();
        v3 = ((uint)(v3 + (uint)(((12)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6A88U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4BA080  void __usercall(unsigned int result@<eax>)
    void F_4BA080(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((32))) * result))));
        if (!(((uint)(((uint)((32))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6EF8U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6EF8U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4BA0C0  char **()
    uint F_4BA0C0()
    {
        uint v0 = 0;
        int v1 = 0;
        uint result = 0;
        uint v3 = 0;
        v0 = ((uint)(M.I32(0x17C6CA4U)));
        v1 = (48);
        L1: ;
        result = M.U32(v0);
        M.WI32(0x17C6CA4U, ((int)(v0)));
        if (!(result != 0)) goto L4;
        if (!(result == ((uint)((-1))))) goto L6;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(v0, v3);
        M.WU32(0x17C696CU, v3);
        result = F_4B9EF0();
        goto L7;
        L6: ;
        result = ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(result - ((uint)((1))))) >> (28)))))))) + ((uint)(((uint)(result - ((uint)((1))))) & ((uint)((268435455)))))));
        M.WU32(v0, result);
        L7: ;
        L4: ;
        Rt::PreAdd(v0, 4);
        Rt::PreAdd(v1, -1);
        L2: ;
        if ((v1 != 0)) goto L1;
        L3: ;
        M.WI32(0x17C6CA4U, ((int)(((uint)(v0 - (uint)(((48)) * 4))))));
        return result;
        return {};
    }

    // sub_4BA130  int()
    int F_4BA130()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        int v3 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6C74U);
        Nat.Memset(M.U32(0x17C6C74U), ((uint)((0))), ((uint)((204))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6C74U), ((uint)((204))));
        v0 = M.U32(0x17C6C74U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((204)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!!((v1 != 0))) goto L3;
        goto LBL_9;
        L3: ;
        if (!(v1 != ((uint)((-1))))) goto L5;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        LBL_9: ;
        M.WI32(0x17C6CA4U, ((int)(((uint)(v0 + (uint)(((3)) * 4))))));
        F_4BA0C0();
        return F_4CEE70();
        L5: ;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        M.WI32(0x17C6CA4U, ((int)(((uint)(M.U32(0x17C6C74U) + (uint)(((3)) * 4))))));
        F_4BA0C0();
        return F_4CEE70();
        return {};
    }

    // sub_4BA1F0  int()
    int F_4BA1F0()
    {
        int v0 = 0;
        uint v1 = 0;
        uint inserted = 0;
        int v3 = 0;
        F_4CEE10((0));
        v0 = ((int)(M.U32(((uint)(M.I32(0x17C6D78U))))));
        if (!(M.U32(((uint)(M.I32(0x17C6D78U)))) != 0)) goto L1;
        if (!((((uint)(v0)) == ((uint)((-1)))) || (((uint)(v0)) == ((uint)((-2)))))) goto L3;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(M.I32(0x17C6D78U))), v1);
        M.WU32(0x17C6C74U, v1);
        if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
        inserted = F_4CEEC0();
        goto L6;
        L5: ;
        inserted = ((uint)((0)));
        L6: ;
        F_4BA130();
        F_4CADB0();
        if (!(inserted != 0)) goto L7;
        M.WU32(inserted, M.U32(((uint)(M.I32(0x17C6D78U)))));
        L7: ;
        goto L4;
        L3: ;
        M.WU32(((uint)(M.I32(0x17C6D78U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
        L4: ;
        L1: ;
        return F_4CEE70();
        return {};
    }

    // sub_4BA280  int()
    int F_4BA280()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        uint v6 = 0;
        uint v7 = 0;
        uint v8 = 0;
        uint v9 = 0;
        uint v10 = 0;
        uint v11 = 0;
        int v12 = 0;
        int v13 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6CD0U);
        Nat.Memset(M.U32(0x17C6CD0U), ((uint)((0))), ((uint)((96))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6CD0U), ((uint)((96))));
        v0 = M.U32(0x17C6CD0U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((96)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WI32(0x17C6BDCU, ((int)(v0)));
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6CD0U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        M.WI32(0x17C6D78U, ((int)(((uint)(v0 + (uint)(((20)) * 4))))));
        F_4BA1F0();
        v3 = M.U32(0x17C6CD0U);
        v4 = M.U32(((uint)(M.U32(0x17C6CD0U) + (uint)(((21)) * 4))));
        if (!(v4 != 0)) goto L7;
        if (!(v4 == ((uint)((-1))))) goto L9;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(M.U32(0x17C6CD0U) + (uint)(((21)) * 4))), v5);
        v13 = ((int)(M.U8(((uint)(v3 + (uint)(((72)) * 1))))));
        M.WU32(0x1EABC3CU, v5);
        M.WU32(0x17C6A88U, v5);
        F_4BA000(v13);
        v3 = M.U32(0x17C6CD0U);
        goto L10;
        L9: ;
        M.WU32(((uint)(M.U32(0x17C6CD0U) + (uint)(((21)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v4 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v4 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L10: ;
        L7: ;
        v6 = M.U32(((uint)(v3 + (uint)(((22)) * 4))));
        if (!(v6 != 0)) goto L11;
        if (!(v6 == ((uint)((-1))))) goto L13;
        v7 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((15))))) & 0xFFFFFFF0U));
        M.WU32(((uint)(v3 + (uint)(((22)) * 4))), v7);
        v8 = ((uint)(M.U8(((uint)(v3 + (uint)(((73)) * 1))))));
        M.WU32(0x1EABC3CU, v7);
        M.WU32(0x17C6EF8U, v7);
        F_4BA080(v8);
        v3 = M.U32(0x17C6CD0U);
        goto L14;
        L13: ;
        M.WU32(((uint)(v3 + (uint)(((22)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v6 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v6 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L14: ;
        L11: ;
        v9 = M.U32(((uint)(v3 + (uint)(((23)) * 4))));
        if (!(v9 != 0)) goto L15;
        if (!(v9 == ((uint)((-1))))) goto L17;
        v10 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(v3 + (uint)(((23)) * 4))), v10);
        v11 = ((uint)(M.U8(((uint)(v3 + (uint)(((74)) * 1))))));
        M.WU32(0x1EABC3CU, v10);
        M.WU32(0x17C6EA4U, v10);
        F_4B9DC0(v11);
        goto L18;
        L17: ;
        M.WU32(((uint)(v3 + (uint)(((23)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v9 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v9 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L18: ;
        L15: ;
        return F_4CEE70();
        return {};
    }

    // sub_4BA420  int()
    int F_4BA420()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint inserted = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(M.U32(0x17C698CU))));
            if (!(M.U32(M.U32(0x17C698CU)) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(M.U32(0x17C698CU), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(M.U32(0x17C698CU), v2);
            M.WU32(0x17C6CD0U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            inserted = F_4CEEC0();
            goto L6;
            L5: ;
            inserted = ((uint)((0)));
            L6: ;
            F_4BA280();
            v4 = M.U32(0x17C698CU);
            M.WI32((_sp + 0x0U), ((int)(M.U32(M.U32(0x17C698CU)))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((5))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(inserted != 0)) goto L7;
            M.WU32(inserted, M.U32(M.U32(0x17C698CU)));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4BA4F0  void __usercall(int result@<eax>, char a2)
    void F_4BA4F0(int result, sbyte a2)
    {
        int v2 = 0;
        int v3 = 0;
        uint v4 = 0;
        v2 = result;
        v3 = ((int)(((uint)((4))) * ((uint)(result))));
        if (!((a2 != 0) && (v3 != 0))) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C698CU), ((uint)((0))), ((uint)(v3)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C698CU), ((uint)(v3)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v3) * 1))));
        L1: ;
        v4 = M.U32(0x17C698CU);
        if (!(v2 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C698CU, v4);
        F_4BA420();
        v4 = ((uint)(v4 + (uint)(((4)) * 1)));
        Rt::PreAdd(v2, -1);
        L8: ;
        if ((v2 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C698CU, ((uint)(v4 + (uint)((((int)(0 - v3))) * 1))));
    }

    // sub_4BAB40  int()
    int F_4BAB40()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        int v3 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6DA8U);
        M.WU32(M.U32(0x17C6DA8U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6DA8U), ((uint)((16))));
        v0 = M.U32(0x17C6DA8U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((16)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6DA8U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        M.WI32(0x17C6C40U, ((int)(((uint)(v0 + (uint)(((1)) * 4))))));
        M.WU32(0x17C6F98U, ((uint)(v0 + (uint)(((1)) * 4))));
        F_4B93A0();
        return F_4CEE70();
        return {};
    }

    // sub_4BABF0  int()
    int F_4BABF0()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x174687CU))))));
            if (!(M.U32(((uint)(M.I32(0x174687CU)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x174687CU))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x174687CU))), v2);
            M.WU32(0x17C6DA8U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4BAB40();
            v4 = ((uint)(M.I32(0x174687CU)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x174687CU)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((22))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x174687CU)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4BAD80  void __cdecl(int a1)
    void F_4BAD80(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((24))) * ((uint)(a1))));
        if (!(((int)(((uint)((24))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6E20U), ((uint)((0))), ((uint)(((int)(((uint)((24))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6E20U), ((uint)(((int)(((uint)((24))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6E20U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6E20U, v3);
        M.WU32(0x17C698CU, ((uint)(v3 + (uint)(((16)) * 1))));
        F_4BA420();
        v3 = ((uint)(v3 + (uint)(((24)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6E20U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4BAE00  void __usercall(int result@<eax>)
    void F_4BAE00(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = result;
        v2 = ((int)(((uint)((8))) * ((uint)(result))));
        if (!(((int)(((uint)((8))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C68E8U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C68E8U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C68E8U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C68E8U, v3);
        M.WU32(0x17C6F98U, v3);
        F_4B93A0();
        M.WU32(0x17C6F98U, ((uint)(M.U32(0x17C68E8U) + (uint)(((4)) * 1))));
        F_4B93A0();
        v3 = ((uint)(v3 + (uint)(((8)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C68E8U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4BB090  void __usercall(unsigned int result@<eax>)
    void F_4BB090(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((20))) * result))));
        if (!(((uint)(((uint)((20))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6B54U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6B54U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4BB0D0  void *()
    uint F_4BB0D0()
    {
        uint v0 = 0;
        byte v1 = 0;
        uint result = 0;
        if (!(M.U32(M.U32(0x17C6EE8U)) == ((uint)((-1))))) goto L1;
        v0 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v1 = ((((uint)(M.I32(0x1EBDC44U))) == ((uint)((2)))) ? (byte)1 : (byte)0);
        M.WU32(0x1EABC3CU, v0);
        M.WU32(M.U32(0x17C6EE8U), v0);
        M.WU32(0x17C6B54U, v0);
        if (!(v1 != 0)) goto L3;
        M.WU32(v0, ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((4)) * 4))), ((uint)((0))));
        result = ((uint)(v0 + (uint)(((5)) * 4)));
        M.WU32(0x1EABC3CU, result);
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(v0, ((uint)((20))));
        result = ((uint)(M.U32(0x1EABC3CU) + (uint)(((20)) * 1)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((20)) * 1))));
        L4: ;
        goto L2;
        L1: ;
        result = ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(M.U32(M.U32(0x17C6EE8U)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(M.U32(M.U32(0x17C6EE8U)) - ((uint)((1))))))) & ((uint)((268435455)))))))))));
        M.WU32(M.U32(0x17C6EE8U), result);
        L2: ;
        return result;
        return {};
    }

    // sub_4BB150  void __usercall(int result@<eax>)
    void F_4BB150(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        byte v4 = 0;
        v1 = result;
        v2 = ((int)(((uint)((8))) * ((uint)(result))));
        if (!(((int)(((uint)((8))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6EE8U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6EE8U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6EE8U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = ((M.U32(v3) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6EE8U, v3);
        if (!!((v4 != 0))) goto L10;
        F_4BB0D0();
        L10: ;
        v3 = ((uint)(v3 + (uint)(((2)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6EE8U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4BB1C0  _BYTE *()
    uint F_4BB1C0()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint result = 0;
        byte v4 = 0;
        int v5 = 0;
        v0 = M.I32(0x17C6F1CU);
        v1 = ((int)(M.U32(((uint)(((int)(((uint)(M.I32(0x17C6F1CU))) + ((uint)((4))))))))));
        if (!(v1 != 0)) goto L1;
        if (!(((uint)(v1)) == ((uint)((-1))))) goto L3;
        v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v2);
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C6F1CU))) + ((uint)((4))))))), v2);
        M.WU32(0x17C6EE8U, v2);
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        M.WU32(v2, ((uint)((0))));
        M.WU32(((uint)(v2 + (uint)(((1)) * 4))), ((uint)((0))));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v2, ((uint)((8))));
        v0 = M.I32(0x17C6F1CU);
        v2 = M.U32(0x1EABC3CU);
        L6: ;
        result = ((uint)(v2 + (uint)(((2)) * 4)));
        v4 = ((M.U32(M.U32(0x17C6EE8U)) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x1EABC3CU, result);
        if (!(v4 != 0)) goto L7;
        goto LBL_10;
        L7: ;
        F_4BB0D0();
        v0 = M.I32(0x17C6F1CU);
        goto L4;
        L3: ;
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C6F1CU))) + ((uint)((4))))))), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L4: ;
        L1: ;
        result = M.U32(0x1EABC3CU);
        LBL_10: ;
        v5 = ((int)(M.U32(((uint)(((int)(((uint)(v0)) + ((uint)((8))))))))));
        if (!(v5 != 0)) goto L9;
        if (!(((uint)(v5)) == ((uint)((-1))))) goto L11;
        M.WU32(0x1EABC3CU, result);
        M.WU32(((uint)(((int)(((uint)(v0)) + ((uint)((8))))))), result);
        M.WI32(0x17C6E9CU, ((int)(result)));
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L13;
        M.WU8(Rt::PostAdd(result, 1), ((byte)(((sbyte)(0)))));
        M.WU32(0x1EABC3CU, result);
        goto L14;
        L13: ;
        Nat.DB_LoadXFileData(result, ((uint)((1))));
        result = ((uint)(M.U32(0x1EABC3CU) + (uint)(((1)) * 1)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((1)) * 1))));
        L14: ;
        goto L12;
        L11: ;
        result = ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v5)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v5)) - ((uint)((1))))))) & ((uint)((268435455)))))))))));
        M.WU32(((uint)(((int)(((uint)(v0)) + ((uint)((8))))))), result);
        L12: ;
        L9: ;
        return result;
        return {};
    }

    // sub_4BB2C0  void()
    void F_4BB2C0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        v0 = ((uint)(M.I32(0x17C6B70U)));
        if (!(M.U32(((uint)(((int)(((uint)(M.I32(0x17C6B70U))) + ((uint)((4)))))))) != 0)) goto L1;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C6B70U))) + ((uint)((4))))))), v1);
        M.WU32(0x17C6EE8U, v1);
        F_4BB150(((int)(M.U16(v0))));
        v0 = ((uint)(M.I32(0x17C6B70U)));
        L1: ;
        if (!(M.U32(((uint)(v0 + (uint)(((2)) * 4)))) != 0)) goto L3;
        v2 = M.U32(0x1EABC3CU);
        v3 = M.U32(0x17C6EBCU);
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), M.U32(0x1EABC3CU));
        v4 = ((int)(M.U32(((uint)(v3 + (uint)(((15)) * 4))))));
        M.WU32(0x1EABC3CU, v2);
        M.WI32(0x17C6E9CU, ((int)(v2)));
        if (!(v4 != 0)) goto L5;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L7;
        Nat.Memset(v2, ((uint)((0))), ((uint)(v4)));
        M.WU32(0x1EABC3CU, ((uint)(v2 + (uint)((v4) * 1))));
        goto L8;
        L7: ;
        Nat.DB_LoadXFileData(v2, ((uint)(v4)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v4) * 1))));
        L8: ;
        L5: ;
        L3: ;
    }

    // sub_4BB360  void()
    void F_4BB360()
    {
        uint v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        Nat.Memset(M.U32(0x17C6EBCU), ((uint)((0))), ((uint)((68))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6EBCU), ((uint)((68))));
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((68)) * 1))));
        M.WI32(0x17C6B70U, ((int)(((uint)(((int)(M.U32(0x17C6EBCU))))) + ((uint)((24))))));
        F_4BB2C0();
        v0 = M.U32(0x17C6EBCU);
        v1 = ((int)(M.U32(((uint)(M.U32(0x17C6EBCU) + (uint)(((16)) * 4))))));
        if (!(v1 != 0)) goto L3;
        if (!(((uint)(v1)) == ((uint)((-1))))) goto L5;
        v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v2);
        M.WU32(((uint)(M.U32(0x17C6EBCU) + (uint)(((16)) * 4))), v2);
        M.WU32(0x17C6B54U, v2);
        F_4BB090(((uint)(M.U16(((uint)(v0 + (uint)(((12)) * 2)))))));
        goto L6;
        L5: ;
        M.WU32(((uint)(M.U32(0x17C6EBCU) + (uint)(((16)) * 4))), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L6: ;
        L3: ;
    }

    // sub_4BB3F0  int()
    int F_4BB3F0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        int v6 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6974U);
        Nat.Memset(M.U32(0x17C6974U), ((uint)((0))), ((uint)((68))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6974U), ((uint)((68))));
        v0 = M.U32(0x17C6974U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((68)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6974U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        v3 = M.U32(((uint)(v0 + (uint)(((7)) * 4))));
        v4 = ((uint)(v0 + (uint)(((7)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v0 + (uint)(((7)) * 4))));
        if (!(v3 != 0)) goto L7;
        if (!(v3 == ((uint)((-1))))) goto L9;
        v5 = M.U32(0x1EABC3CU);
        M.WU32(v4, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v5)));
        Nat.Load_XStringData(v4);
        goto L10;
        L9: ;
        M.WU32(v4, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v3 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v3 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L10: ;
        L7: ;
        return F_4CEE70();
        return {};
    }

    // sub_4BB4D0  int()
    int F_4BB4D0()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6F08U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6F08U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6F08U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6F08U))), v2);
            M.WU32(0x17C6974U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4BB3F0();
            v4 = ((uint)(M.I32(0x17C6F08U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6F08U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((0))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6F08U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4BB5C0  void __cdecl(int a1)
    void F_4BB5C0(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        byte v4 = 0;
        uint v5 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((68))) * ((uint)(a1))));
        if (!(((int)(((uint)((68))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6F64U), ((uint)((0))), ((uint)(((int)(((uint)((68))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6F64U), ((uint)(((int)(((uint)((68))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6F64U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        v4 = ((M.U32(v3) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6F64U, v3);
        if (!!((v4 != 0))) goto L10;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v5);
        M.WU32(v3, v5);
        M.WU32(0x17C6EBCU, v5);
        F_4BB360();
        L10: ;
        v3 = ((uint)(v3 + (uint)(((17)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6F64U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4BB650  int()
    int F_4BB650()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x1746864U);
        Nat.Memset(M.U32(0x1746864U), ((uint)((0))), ((uint)((72))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x1746864U), ((uint)((72))));
        v0 = M.U32(0x1746864U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((72)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x1746864U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((2)) * 4)))) != 0)) goto L7;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), v3);
        M.WU32(0x17C6F64U, v3);
        F_4BB5C0(((int)(M.U32(((uint)(v0 + (uint)(((1)) * 4)))))));
        L7: ;
        return F_4CEE70();
        return {};
    }

    // sub_4BB710  int()
    int F_4BB710()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6F6CU))))));
            if (!(M.U32(((uint)(M.I32(0x17C6F6CU)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6F6CU))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6F6CU))), v2);
            M.WU32(0x1746864U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4BB650();
            v4 = ((uint)(M.I32(0x17C6F6CU)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6F6CU)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((1))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6F6CU)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4BB890  int()
    int F_4BB890()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C69E4U);
        Nat.Memset(M.U32(0x17C69E4U), ((uint)((0))), ((uint)((48))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C69E4U), ((uint)((48))));
        v0 = M.U32(0x17C69E4U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((48)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C69E4U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((1)) * 4)))) != 0)) goto L7;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), v3);
        M.WU32(0x17C6D1CU, v3);
        F_4B9180(((int)(M.U16(((uint)(v0 + (uint)(((4)) * 2)))))));
        L7: ;
        return F_4CEE70();
        return {};
    }

    // sub_4BB950  int()
    int F_4BB950()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint inserted = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6F68U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6F68U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6F68U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6F68U))), v2);
            M.WU32(0x17C69E4U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            inserted = F_4CEEC0();
            goto L6;
            L5: ;
            inserted = ((uint)((0)));
            L6: ;
            F_4BB890();
            v4 = ((uint)(M.I32(0x17C6F68U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6F68U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((3))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(inserted != 0)) goto L7;
            M.WU32(inserted, M.U32(((uint)(M.I32(0x17C6F68U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4BBA20  char **()
    uint F_4BBA20()
    {
        int v0 = 0;
        int v1 = 0;
        int v2 = 0;
        uint result = 0;
        uint v4 = 0;
        M.WI32(0x17C6F68U, ((int)(((uint)(M.I32(0x17C6AA4U))) + ((uint)((8))))));
        F_4BB950();
        v0 = M.I32(0x17C6AA4U);
        v1 = ((int)(M.U32(((uint)(((int)(((uint)(M.I32(0x17C6AA4U))) + ((uint)((8))))))))));
        if (!(v1 != 0)) goto L1;
        M.Memcpy(((uint)(((int)(((uint)(M.I32(0x17C6AA4U))) + ((uint)((16))))))), ((uint)(((int)(((uint)(v1)) + ((uint)((12))))))), 0x20U);
        M.WU32(((uint)(((int)(((uint)(v0)) + ((uint)((48))))))), M.U32(((uint)(((int)(((uint)(v1)) + ((uint)((4)))))))));
        L1: ;
        v2 = ((int)(M.U32(((uint)(((int)(((uint)(v0)) + ((uint)((12))))))))));
        result = ((uint)(((int)(((uint)(v0)) + ((uint)((12)))))));
        M.WU32(0x17C6EECU, result);
        if (!(v2 != 0)) goto L3;
        if (!(((uint)(v2)) == ((uint)((-1))))) goto L5;
        v4 = M.U32(0x1EABC3CU);
        M.WU32(result, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v4)));
        return ((uint)(Nat.Load_XStringData(result)));
        goto L6;
        L5: ;
        M.WU32(result, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v2)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v2)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L6: ;
        L3: ;
        return result;
        return {};
    }

    // sub_4BBAA0  void __cdecl(signed int a1)
    void F_4BBAA0(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        byte v5 = 0;
        int v6 = 0;
        int v7 = 0;
        int v8 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((44))) * ((uint)(a1))));
        v7 = ((int)(((uint)((44))) * ((uint)(a1))));
        if (!(((int)(((uint)((44))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6938U), ((uint)((0))), ((uint)(((int)(((uint)((44))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6938U), ((uint)(((int)(((uint)((44))) * ((uint)(a1)))))));
        L4: ;
        v1 = a1;
        v3 = ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        goto L2;
        L1: ;
        v3 = M.U32(0x1EABC3CU);
        L2: ;
        v4 = M.U32(0x17C6938U);
        if (!(v1 > (0))) goto L5;
        v8 = v1;
        L7: ;
        v5 = ((M.U32(v4) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6938U, v4);
        if (!!((v5 != 0))) goto L10;
        v3 = ((uint)(((uint)(v3 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v6 = ((int)(((uint)((48))) * M.U32(((uint)(v4 + (uint)(((1)) * 4))))));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(v4, v3);
        M.WI32(0x17C6BB4U, ((int)(v3)));
        if (!(v6 != 0)) goto L12;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L14;
        Nat.Memset(v3, ((uint)((0))), ((uint)(v6)));
        goto L15;
        L14: ;
        Nat.DB_LoadXFileData(v3, ((uint)(v6)));
        v3 = M.U32(0x1EABC3CU);
        L15: ;
        v3 = ((uint)(v3 + (uint)((v6) * 1)));
        M.WU32(0x1EABC3CU, v3);
        L12: ;
        v2 = v7;
        L10: ;
        v4 = ((uint)(v4 + (uint)(((11)) * 4)));
        Rt::PreAdd(v8, -1);
        L8: ;
        if ((v8 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6938U, ((uint)(v4 - (uint)((v2) * 1))));
    }

    // sub_4BBB80  int()
    int F_4BBB80()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        uint v6 = 0;
        uint v7 = 0;
        int v8 = 0;
        uint v9 = 0;
        uint v10 = 0;
        int v11 = 0;
        uint v12 = 0;
        uint v13 = 0;
        uint v14 = 0;
        int v15 = 0;
        uint v16 = 0;
        uint v17 = 0;
        uint v18 = 0;
        int v19 = 0;
        int v20 = 0;
        uint v21 = 0;
        uint v22 = 0;
        uint v23 = 0;
        uint v24 = 0;
        int v25 = 0;
        int v26 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6C44U);
        Nat.Memset(M.U32(0x17C6C44U), ((uint)((0))), ((uint)((360))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6C44U), ((uint)((360))));
        v0 = M.U32(0x17C6C44U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((360)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6C44U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        v3 = M.U32(((uint)(v0 + (uint)(((11)) * 4))));
        if (!(v3 != 0)) goto L7;
        if (!(v3 == ((uint)((-1))))) goto L9;
        v4 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((1))))) & 0xFFFFFFFEU));
        M.WU32(0x1EABC3CU, v4);
        M.WU32(((uint)(v0 + (uint)(((11)) * 4))), v4);
        M.WU32(0x17C6F3CU, v4);
        F_4B6F10(((int)(M.U8(((uint)(v0 + (uint)(((4)) * 1)))))), ((sbyte)((1))));
        v0 = M.U32(0x17C6C44U);
        goto L10;
        L9: ;
        M.WU32(((uint)(v0 + (uint)(((11)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v3 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v3 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L10: ;
        L7: ;
        v5 = M.U32(((uint)(v0 + (uint)(((12)) * 4))));
        if (!(v5 != 0)) goto L11;
        if (!(v5 == ((uint)((-1))))) goto L13;
        v6 = M.U32(0x1EABC3CU);
        M.WU32(((uint)(v0 + (uint)(((12)) * 4))), M.U32(0x1EABC3CU));
        M.WU32(0x1EABC3CU, v6);
        M.WU32(0x17C6AFCU, v6);
        F_4B6C50(((int)(((uint)(M.U8(((uint)(v0 + (uint)(((4)) * 1)))))) - ((uint)(M.U8(((uint)(v0 + (uint)(((5)) * 1)))))))));
        v0 = M.U32(0x17C6C44U);
        goto L14;
        L13: ;
        M.WU32(((uint)(v0 + (uint)(((12)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v5 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v5 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L14: ;
        L11: ;
        v7 = M.U32(((uint)(v0 + (uint)(((13)) * 4))));
        if (!(v7 != 0)) goto L15;
        if (!(v7 == ((uint)((-1))))) goto L17;
        v8 = ((int)(M.U8(((uint)(v0 + (uint)(((5)) * 1))))));
        v9 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((1))))) & 0xFFFFFFFEU));
        M.WU32(0x1EABC3CU, v9);
        M.WU32(((uint)(v0 + (uint)(((13)) * 4))), v9);
        M.WU32(0x17C6F8CU, v9);
        F_4B6E10(((uint)(((int)(((uint)((4))) * ((uint)(((int)(((uint)(M.U8(((uint)(v0 + (uint)(((4)) * 1)))))) - ((uint)(v8)))))))))));
        v0 = M.U32(0x17C6C44U);
        goto L18;
        L17: ;
        M.WU32(((uint)(v0 + (uint)(((13)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v7 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v7 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L18: ;
        L15: ;
        v10 = M.U32(((uint)(v0 + (uint)(((14)) * 4))));
        if (!(v10 != 0)) goto L19;
        if (!(v10 == ((uint)((-1))))) goto L21;
        v11 = ((int)(M.U8(((uint)(v0 + (uint)(((5)) * 1))))));
        v12 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v12);
        M.WU32(((uint)(v0 + (uint)(((14)) * 4))), v12);
        M.WU32(0x1746868U, v12);
        F_4B6D10(((int)(((uint)((3))) * ((uint)(((int)(((uint)(M.U8(((uint)(v0 + (uint)(((4)) * 1)))))) - ((uint)(v11)))))))));
        v0 = M.U32(0x17C6C44U);
        goto L22;
        L21: ;
        M.WU32(((uint)(v0 + (uint)(((14)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v10 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v10 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L22: ;
        L19: ;
        v13 = M.U32(((uint)(v0 + (uint)(((15)) * 4))));
        if (!(v13 != 0)) goto L23;
        if (!(v13 == ((uint)((-1))))) goto L25;
        v14 = M.U32(0x1EABC3CU);
        M.WU32(((uint)(v0 + (uint)(((15)) * 4))), M.U32(0x1EABC3CU));
        v15 = ((int)(M.U8(((uint)(v0 + (uint)(((4)) * 1))))));
        M.WU32(0x1EABC3CU, v14);
        M.WU32(0x17C6AFCU, v14);
        F_4B6C50(v15);
        v0 = M.U32(0x17C6C44U);
        goto L26;
        L25: ;
        M.WU32(((uint)(v0 + (uint)(((15)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v13 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v13 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L26: ;
        L23: ;
        v16 = M.U32(((uint)(v0 + (uint)(((16)) * 4))));
        if (!(v16 != 0)) goto L27;
        if (!(v16 == ((uint)((-1))))) goto L29;
        v17 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v17);
        M.WU32(((uint)(v0 + (uint)(((16)) * 4))), v17);
        M.WU32(0x17C6924U, v17);
        F_4B7EF0(((uint)(M.U8(((uint)(v0 + (uint)(((4)) * 1)))))));
        v0 = M.U32(0x17C6C44U);
        goto L30;
        L29: ;
        M.WU32(((uint)(v0 + (uint)(((16)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v16 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v16 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L30: ;
        L27: ;
        if (!(M.U32(((uint)(v0 + (uint)(((17)) * 4)))) != 0)) goto L31;
        v18 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v18);
        M.WU32(((uint)(v0 + (uint)(((17)) * 4))), v18);
        M.WU32(0x17C698CU, v18);
        F_4BA4F0(((int)(M.U8(((uint)(v0 + (uint)(((6)) * 1)))))), ((sbyte)((1))));
        v0 = M.U32(0x17C6C44U);
        L31: ;
        v19 = ((int)(((uint)(v0 + (uint)(((18)) * 4)))));
        v20 = (4);
        L33: ;
        M.WI32(0x17C6AA4U, v19);
        F_4BBA20();
        v19 = ((int)(((uint)(v19)) + ((uint)((56)))));
        Rt::PreAdd(v20, -1);
        L34: ;
        if ((v20 != 0)) goto L33;
        L35: ;
        v21 = M.U32(0x17C6C44U);
        M.WI32(0x17C6AA4U, ((int)(((uint)(v19)) - ((uint)((224))))));
        if (!(M.U32(((uint)(M.U32(0x17C6C44U) + (uint)(((75)) * 4)))) != 0)) goto L36;
        v22 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(M.U32(0x17C6C44U) + (uint)(((75)) * 4))), v22);
        v26 = ((int)(M.U32(((uint)(v21 + (uint)(((76)) * 4))))));
        M.WU32(0x1EABC3CU, v22);
        M.WU32(0x17C6938U, v22);
        F_4BBAA0(v26);
        v21 = M.U32(0x17C6C44U);
        L36: ;
        if (!(M.U32(((uint)(v21 + (uint)(((78)) * 4)))) != 0)) goto L38;
        v23 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(v21 + (uint)(((78)) * 4))), v23);
        v24 = ((uint)(M.U8(((uint)(v21 + (uint)(((4)) * 1))))));
        M.WU32(0x1EABC3CU, v23);
        M.WU32(0x17C6A2CU, v23);
        F_4B7EA0(v24);
        v21 = M.U32(0x17C6C44U);
        L38: ;
        M.WI32(0x17C6F08U, ((int)(((uint)(v21 + (uint)(((88)) * 4))))));
        F_4BB4D0();
        M.WI32(0x17C6F6CU, ((int)(((uint)(M.U32(0x17C6C44U) + (uint)(((89)) * 4))))));
        F_4BB710();
        return F_4CEE70();
        return {};
    }

    // sub_4BBEF0  int()
    int F_4BBEF0()
    {
        int v0 = 0;
        uint v1 = 0;
        uint inserted = 0;
        int v3 = 0;
        F_4CEE10((0));
        v0 = ((int)(M.U32(((uint)(M.I32(0x17C69D4U))))));
        if (!(M.U32(((uint)(M.I32(0x17C69D4U)))) != 0)) goto L1;
        if (!((((uint)(v0)) == ((uint)((-1)))) || (((uint)(v0)) == ((uint)((-2)))))) goto L3;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(M.I32(0x17C69D4U))), v1);
        M.WU32(0x17C6C44U, v1);
        if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
        inserted = F_4CEEC0();
        goto L6;
        L5: ;
        inserted = ((uint)((0)));
        L6: ;
        F_4BBB80();
        F_4CACE0();
        if (!(inserted != 0)) goto L7;
        M.WU32(inserted, M.U32(((uint)(M.I32(0x17C69D4U)))));
        L7: ;
        goto L4;
        L3: ;
        M.WU32(((uint)(M.I32(0x17C69D4U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
        L4: ;
        L1: ;
        return F_4CEE70();
        return {};
    }

    // sub_4BC390  void()
    void F_4BC390()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        int v3 = 0;
        v0 = M.U32(0x17C6B98U);
        if (!(M.U32(((uint)(M.U32(0x17C6B98U) + (uint)(((13)) * 4)))) != 0)) goto L1;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(M.U32(0x17C6B98U) + (uint)(((13)) * 4))), v1);
        v2 = ((int)(M.U32(((uint)(v0 + (uint)(((14)) * 4))))));
        v3 = ((int)(((uint)((12))) * ((uint)(v2))));
        M.WU32(0x1EABC3CU, v1);
        M.WI32(0x17C6C1CU, ((int)(v1)));
        if (!(((int)(((uint)((12))) * ((uint)(v2)))) != 0)) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(v1, ((uint)((0))), ((uint)(((int)(((uint)((12))) * ((uint)(v2)))))));
        M.WU32(0x1EABC3CU, ((uint)(v1 + (uint)((v3) * 1))));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v1, ((uint)(((int)(((uint)((12))) * ((uint)(v2)))))));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v3) * 1))));
        L6: ;
        L3: ;
        L1: ;
    }

    // sub_4BC410  void __cdecl(int a1)
    void F_4BC410(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((60))) * ((uint)(a1))));
        if (!(((int)(((uint)((60))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6B98U), ((uint)((0))), ((uint)(((int)(((uint)((60))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6B98U), ((uint)(((int)(((uint)((60))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6B98U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6B98U, v3);
        F_4BC390();
        v3 = ((uint)(v3 + (uint)(((60)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6B98U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4BC480  void __usercall(int result@<eax>)
    void F_4BC480(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        uint v5 = 0;
        v1 = result;
        v2 = ((int)(((uint)((4))) * ((uint)(result))));
        if (!(((int)(((uint)((4))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6D9CU), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6D9CU), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6D9CU);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = M.I32(v3);
        M.WU32(0x17C6D9CU, v3);
        if (!(v4 != 0)) goto L10;
        if (!(((uint)(v4)) == ((uint)((-1))))) goto L12;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v5);
        M.WI32(v3, ((int)(v5)));
        M.WU32(0x17C6E80U, v5);
        F_4BC520(((sbyte)((1))));
        goto L13;
        L12: ;
        M.WI32(v3, ((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v4)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v4)) - ((uint)((1))))))) & ((uint)((268435455))))))))));
        L13: ;
        L10: ;
        Rt::PreAdd(v3, 4);
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6D9CU, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4BC520  void __cdecl(char a1)
    void F_4BC520(sbyte a1)
    {
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        uint v6 = 0;
        if (!(a1 != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        v1 = M.U32(0x17C6E80U);
        Nat.Memset(M.U32(0x17C6E80U), ((uint)((0))), ((uint)((44))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6E80U), ((uint)((44))));
        v1 = M.U32(0x17C6E80U);
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((44)) * 1))));
        goto L2;
        L1: ;
        v1 = M.U32(0x17C6E80U);
        L2: ;
        v2 = M.U32(v1);
        M.WU32(0x17C6EECU, v1);
        if (!(v2 != 0)) goto L5;
        if (!(v2 == ((uint)((-1))))) goto L7;
        v3 = M.U32(0x1EABC3CU);
        M.WU32(v1, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v3)));
        Nat.Load_XStringData(v1);
        v1 = M.U32(0x17C6E80U);
        goto L8;
        L7: ;
        M.WU32(v1, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v2 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v2 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L8: ;
        L5: ;
        if (!(M.U32(((uint)(v1 + (uint)(((1)) * 4)))) != 0)) goto L9;
        v4 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v4);
        M.WU32(((uint)(v1 + (uint)(((1)) * 4))), v4);
        M.WU32(0x17C6B98U, v4);
        F_4BC410(((int)(M.U32(((uint)(v1 + (uint)(((2)) * 4)))))));
        v1 = M.U32(0x17C6E80U);
        L9: ;
        if (!(M.U32(((uint)(v1 + (uint)(((3)) * 4)))) != 0)) goto L11;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v5);
        M.WU32(((uint)(v1 + (uint)(((3)) * 4))), v5);
        M.WU32(0x17C6D9CU, v5);
        F_4BC480(((int)(M.U32(((uint)(v1 + (uint)(((4)) * 4)))))));
        v1 = M.U32(0x17C6E80U);
        L11: ;
        if (!(M.U32(((uint)(v1 + (uint)(((5)) * 4)))) != 0)) goto L13;
        v6 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v6);
        M.WU32(((uint)(v1 + (uint)(((5)) * 4))), v6);
        M.WU32(0x17C6D9CU, v6);
        F_4BC480(((int)(M.U32(((uint)(v1 + (uint)(((6)) * 4)))))));
        L13: ;
    }

    // sub_4BC650  void __cdecl(int a1)
    void F_4BC650(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((44))) * ((uint)(a1))));
        if (!(((int)(((uint)((44))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6E80U), ((uint)((0))), ((uint)(((int)(((uint)((44))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6E80U), ((uint)(((int)(((uint)((44))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6E80U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6E80U, v3);
        F_4BC520(((sbyte)((0))));
        v3 = ((uint)(v3 + (uint)(((11)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6E80U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4BC6D0  void()
    void F_4BC6D0()
    {
        uint v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        int v3 = 0;
        uint v4 = 0;
        byte v5 = 0;
        int v6 = 0;
        v0 = M.U32(0x17C6998U);
        v1 = ((int)(M.U32(M.U32(0x17C6998U))));
        M.WU32(0x17C6EECU, M.U32(0x17C6998U));
        if (!(v1 != 0)) goto L1;
        if (!(((uint)(v1)) == ((uint)((-1))))) goto L3;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(M.U32(0x17C6998U), M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6998U);
        goto L4;
        L3: ;
        M.WU32(M.U32(0x17C6998U), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L4: ;
        L1: ;
        if (!(M.U32(((uint)(v0 + (uint)(((2)) * 4)))) != 0)) goto L5;
        v3 = ((int)(M.U16(((uint)(v0 + (uint)(((3)) * 2))))));
        v4 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((1))))) & 0xFFFFFFFEU));
        v5 = ((((uint)(((int)(((uint)((2))) * ((uint)(v3)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v6 = ((int)(((uint)((2))) * ((uint)(v3))));
        M.WU32(0x1EABC3CU, v4);
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), v4);
        M.WU32(0x17C69ACU, v4);
        if (!!((v5 != 0))) goto L7;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L9;
        Nat.Memset(v4, ((uint)((0))), ((uint)(v6)));
        M.WU32(0x1EABC3CU, ((uint)(v4 + (uint)((v6) * 1))));
        goto L10;
        L9: ;
        Nat.DB_LoadXFileData(v4, ((uint)(v6)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v6) * 1))));
        L10: ;
        L7: ;
        L5: ;
    }

    // sub_4BC790  void __cdecl(int a1)
    void F_4BC790(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((12))) * ((uint)(a1))));
        if (!(((int)(((uint)((12))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6998U), ((uint)((0))), ((uint)(((int)(((uint)((12))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6998U), ((uint)(((int)(((uint)((12))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6998U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6998U, v3);
        F_4BC6D0();
        v3 = ((uint)(v3 + (uint)(((3)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6998U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4BC800  void()
    void F_4BC800()
    {
        uint v0 = 0;
        uint v1 = 0;
        byte v2 = 0;
        int v3 = 0;
        uint v4 = 0;
        int v5 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6EACU);
        Nat.Memset(M.U32(0x17C6EACU), ((uint)((0))), ((uint)((128))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6EACU), ((uint)((128))));
        v0 = M.U32(0x17C6EACU);
        L2: ;
        v1 = ((uint)(M.U32(0x1EABC3CU) + (uint)(((128)) * 1)));
        v2 = ((M.U32(v0) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((128)) * 1))));
        if (!!((v2 != 0))) goto L3;
        v1 = ((uint)(((uint)(v1 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v2 = ((((uint)(((int)(((uint)((12))) * M.U32(((uint)(v0 + (uint)(((1)) * 4)))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v3 = ((int)(((uint)((12))) * M.U32(((uint)(v0 + (uint)(((1)) * 4))))));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(v0, v1);
        M.WI32(0x17C6AECU, ((int)(v1)));
        if (!!((v2 != 0))) goto L5;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L7;
        Nat.Memset(v1, ((uint)((0))), ((uint)(v3)));
        goto L8;
        L7: ;
        Nat.DB_LoadXFileData(v1, ((uint)(v3)));
        v0 = M.U32(0x17C6EACU);
        v1 = M.U32(0x1EABC3CU);
        L8: ;
        v1 = ((uint)(v1 + (uint)((v3) * 1)));
        M.WU32(0x1EABC3CU, v1);
        L5: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((4)) * 4)))) != 0)) goto L9;
        v4 = ((uint)(((uint)(v1 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v5 = ((int)(M.U32(((uint)(v0 + (uint)(((3)) * 4))))));
        M.WU32(0x1EABC3CU, v4);
        M.WU32(((uint)(v0 + (uint)(((4)) * 4))), v4);
        M.WU32(0x17C6998U, v4);
        F_4BC790(v5);
        L9: ;
    }

    // sub_4BC8D0  int()
    int F_4BC8D0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        uint v6 = 0;
        int v7 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6C8CU);
        Nat.Memset(M.U32(0x17C6C8CU), ((uint)((0))), ((uint)((84))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6C8CU), ((uint)((84))));
        v0 = M.U32(0x17C6C8CU);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((84)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6C8CU);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        M.WI32(0x17C6BF8U, ((int)(((uint)(v0 + (uint)(((1)) * 4))))));
        if (!(v0 != ((uint)((-4))))) goto L7;
        Nat.Memset(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))), ((uint)((68))));
        L7: ;
        v3 = M.U32(((uint)(v0 + (uint)(((18)) * 4))));
        v4 = ((uint)(v0 + (uint)(((18)) * 4)));
        M.WI32(0x17C6950U, ((int)(((uint)(v0 + (uint)(((18)) * 4))))));
        if (!(v3 != 0)) goto L9;
        if (!(v3 == ((uint)((-1))))) goto L11;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v5);
        M.WU32(v4, v5);
        M.WU32(0x17C6E80U, v5);
        F_4BC650(((int)(M.U32(((uint)(v0 + (uint)(((19)) * 4)))))));
        v0 = M.U32(0x17C6C8CU);
        goto L12;
        L11: ;
        M.WU32(v4, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v3 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v3 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L12: ;
        L9: ;
        if (!(M.U32(((uint)(v0 + (uint)(((20)) * 4)))) != 0)) goto L13;
        v6 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v6);
        M.WU32(((uint)(v0 + (uint)(((20)) * 4))), v6);
        M.WU32(0x17C6EACU, v6);
        F_4BC800();
        L13: ;
        return F_4CEE70();
        return {};
    }

    // sub_4BCA00  int()
    int F_4BCA00()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        uint v6 = 0;
        int v7 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6EE0U);
        Nat.Memset(M.U32(0x17C6EE0U), ((uint)((0))), ((uint)((84))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6EE0U), ((uint)((84))));
        v0 = M.U32(0x17C6EE0U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((84)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6EE0U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        M.WI32(0x17C6BF8U, ((int)(((uint)(v0 + (uint)(((1)) * 4))))));
        if (!(v0 != ((uint)((-4))))) goto L7;
        Nat.Memset(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))), ((uint)((68))));
        L7: ;
        v3 = M.U32(((uint)(v0 + (uint)(((18)) * 4))));
        v4 = ((uint)(v0 + (uint)(((18)) * 4)));
        M.WI32(0x17C6950U, ((int)(((uint)(v0 + (uint)(((18)) * 4))))));
        if (!(v3 != 0)) goto L9;
        if (!(v3 == ((uint)((-1))))) goto L11;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v5);
        M.WU32(v4, v5);
        M.WU32(0x17C6E80U, v5);
        F_4BC650(((int)(M.U32(((uint)(v0 + (uint)(((19)) * 4)))))));
        v0 = M.U32(0x17C6EE0U);
        goto L12;
        L11: ;
        M.WU32(v4, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v3 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v3 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L12: ;
        L9: ;
        if (!(M.U32(((uint)(v0 + (uint)(((20)) * 4)))) != 0)) goto L13;
        v6 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v6);
        M.WU32(((uint)(v0 + (uint)(((20)) * 4))), v6);
        M.WU32(0x17C6EACU, v6);
        F_4BC800();
        L13: ;
        return F_4CEE70();
        return {};
    }

    // sub_4BCB30  int()
    int F_4BCB30()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C68F0U))))));
            if (!(M.U32(((uint)(M.I32(0x17C68F0U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C68F0U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C68F0U))), v2);
            M.WU32(0x17C6C8CU, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4BC8D0();
            v4 = ((uint)(M.I32(0x17C68F0U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C68F0U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((17))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C68F0U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4BCC20  int()
    int F_4BCC20()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6E2CU))))));
            if (!(M.U32(((uint)(M.I32(0x17C6E2CU)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6E2CU))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6E2CU))), v2);
            M.WU32(0x17C6EE0U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4BCA00();
            v4 = ((uint)(M.I32(0x17C6E2CU)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6E2CU)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((18))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6E2CU)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4BCDF0  int()
    int F_4BCDF0()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint inserted = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6B88U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6B88U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6B88U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6B88U))), v2);
            M.WU32(0x17C6BECU, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            inserted = F_4CEEC0();
            goto L6;
            L5: ;
            inserted = ((uint)((0)));
            L6: ;
            F_4BD5E0();
            v4 = ((uint)(M.I32(0x17C6B88U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6B88U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((30))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(inserted != 0)) goto L7;
            M.WU32(inserted, M.U32(((uint)(M.I32(0x17C6B88U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4BCEC0  void __cdecl(int a1)
    void F_4BCEC0(int a1)
    {
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        byte v5 = 0;
        int v6 = 0;
        v1 = ((int)(((uint)((8))) * ((uint)(a1))));
        v6 = ((int)(((uint)((8))) * ((uint)(a1))));
        if (!(((int)(((uint)((8))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C69F8U), ((uint)((0))), ((uint)(((int)(((uint)((8))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C69F8U), ((uint)(((int)(((uint)((8))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
        v2 = M.U32(0x17C69F8U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C69F8U, v2);
        v3 = v2;
        v4 = (2);
        L10: ;
        M.WU32(0x17C698CU, v3);
        F_4BA420();
        v3 = ((uint)(v3 + (uint)(((4)) * 1)));
        Rt::PreAdd(v4, -1);
        L11: ;
        if ((v4 != 0)) goto L10;
        L12: ;
        v2 = ((uint)(v2 + (uint)(((8)) * 1)));
        v5 = ((((uint)(Rt::PostAdd(a1, -1))) == ((uint)((1)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C698CU, ((uint)(v3 - (uint)(((8)) * 1))));
        L8: ;
        if (!((v5 != 0))) goto L7;
        L9: ;
        v1 = v6;
        L5: ;
        M.WU32(0x17C69F8U, ((uint)(v2 + (uint)((((int)(0 - v1))) * 1))));
    }

    // sub_4BCF60  char()
    sbyte F_4BCF60()
    {
        int v0 = 0;
        uint v1 = 0;
        sbyte v2 = 0;
        uint _t1 = 0;
        Rt::Discard(((byte)((ulong)(v0 = ((int)((((ulong)(v0)) & ~0xFFULL) | ((((ulong)(M.U8(((uint)(M.U32(0x17C6D44U) + (uint)(((176)) * 1)))))) & 0xFFULL) << 0)))) >> 0)));
        _t1 = ((uint)(((byte)(v0))));
        if (_t1 == 0x9U) goto L2;
        if (_t1 == 0xEU) goto L3;
        if (_t1 == 0xCU) goto L4;
        goto L5;
        L2: ;
        M.WI32(0x17C69D4U, ((int)(M.U32(0x17C6BE4U))));
        Rt::Discard(((byte)((ulong)(v0 = ((int)((((ulong)(v0)) & ~0xFFULL) | ((((ulong)(((byte)(F_4BBEF0())))) & 0xFFULL) << 0)))) >> 0)));
        goto L1;
        L3: ;
        M.WI32(0x17C6AF8U, ((int)(M.U32(0x17C6BE4U))));
        M.WU32(0x17C6EECU, M.U32(0x17C6BE4U));
        Rt::Discard(((byte)((ulong)(v0 = ((int)((((ulong)(v0)) & ~0xFFULL) | ((((ulong)(((byte)(F_4B7070(((sbyte)((0)))))))) & 0xFFULL) << 0)))) >> 0)));
        v1 = ((uint)(M.I32(0x17C6AF8U)));
        if (!(M.U32(((uint)(M.I32(0x17C6AF8U)))) != 0)) goto L6;
        v0 = ((int)(Nat.DB_FindXAssetHeader(((uint)((30))), M.U32(((uint)(M.I32(0x17C6AF8U)))))));
        M.WI32(v1, v0);
        L6: ;
        goto L1;
        L4: ;
        M.WU32(0x17C6EECU, M.U32(0x17C6BE4U));
        Rt::Discard(((byte)((ulong)(v0 = ((int)((((ulong)(v0)) & ~0xFFULL) | ((((ulong)(((byte)(F_4B7070(((sbyte)((0)))))))) & 0xFFULL) << 0)))) >> 0)));
        goto L1;
        L5: ;
        if (!((((uint)(((byte)(v0)))) != ((uint)(((sbyte)(10))))) && (((uint)(((byte)(v0)))) != ((uint)(((sbyte)(11))))))) goto L8;
        M.WU32(0x17C698CU, M.U32(0x17C6BE4U));
        Rt::Discard(((byte)((ulong)(v0 = ((int)((((ulong)(v0)) & ~0xFFULL) | ((((ulong)(((byte)(F_4BA420())))) & 0xFFULL) << 0)))) >> 0)));
        L8: ;
        goto L1;
        L1: ;
        return ((sbyte)(v0));
        return {};
    }

    // sub_4BCFF0  void __usercall(int a1@<eax>)
    void F_4BCFF0(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((4))) * ((uint)(a1))));
        if (!(((int)(((uint)((4))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6BE4U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6BE4U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6BE4U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6BE4U, v3);
        F_4BCF60();
        v3 = ((uint)(v3 + (uint)(((4)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6BE4U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4BD050  void()
    void F_4BD050()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        v0 = M.U32(0x17C6D44U);
        if (!(((uint)(M.U8(((uint)(M.U32(0x17C6D44U) + (uint)(((176)) * 1)))))) == ((uint)(((sbyte)(13)))))) goto L1;
        if (!(M.U32(((uint)(M.I32(0x17C6930U)))) != 0)) goto L3;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(M.I32(0x17C6930U))), v1);
        M.WU32(0x17C69F8U, v1);
        F_4BCEC0(((int)(M.U8(((uint)(v0 + (uint)(((177)) * 1)))))));
        L3: ;
        goto L2;
        L1: ;
        if (!(((uint)(M.U8(((uint)(M.U32(0x17C6D44U) + (uint)(((177)) * 1)))))) > 0x1U)) goto L5;
        if (!(M.U32(((uint)(M.I32(0x17C6930U)))) != 0)) goto L7;
        v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v2);
        M.WU32(((uint)(M.I32(0x17C6930U))), v2);
        M.WU32(0x17C6BE4U, v2);
        F_4BCFF0(((int)(M.U8(((uint)(v0 + (uint)(((177)) * 1)))))));
        L7: ;
        goto L6;
        L5: ;
        M.WU32(0x17C6BE4U, ((uint)(M.I32(0x17C6930U))));
        F_4BCF60();
        L6: ;
        L2: ;
    }

    // sub_4BD0E0  void()
    void F_4BD0E0()
    {
        uint v0 = 0;
        uint v1 = 0;
        byte v2 = 0;
        int v3 = 0;
        uint v4 = 0;
        int v5 = 0;
        int v6 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6EC8U);
        M.WU32(M.U32(0x17C6EC8U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((4)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((5)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((6)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((7)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((8)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6EC8U), ((uint)((36))));
        v0 = M.U32(0x17C6EC8U);
        L2: ;
        v1 = ((uint)(M.U32(0x1EABC3CU) + (uint)(((36)) * 1)));
        v2 = ((M.U32(((uint)(v0 + (uint)(((6)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((36)) * 1))));
        if (!!((v2 != 0))) goto L3;
        v1 = ((uint)(((uint)(v1 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v2 = ((((uint)(((int)(((uint)((20))) * M.U32(((uint)(v0 + (uint)(((5)) * 4)))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v3 = ((int)(((uint)((20))) * M.U32(((uint)(v0 + (uint)(((5)) * 4))))));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(v0 + (uint)(((6)) * 4))), v1);
        M.WI32(0x17C6B04U, ((int)(v1)));
        if (!!((v2 != 0))) goto L5;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L7;
        Nat.Memset(v1, ((uint)((0))), ((uint)(v3)));
        goto L8;
        L7: ;
        Nat.DB_LoadXFileData(v1, ((uint)(v3)));
        v0 = M.U32(0x17C6EC8U);
        v1 = M.U32(0x1EABC3CU);
        L8: ;
        v1 = ((uint)(v1 + (uint)((v3) * 1)));
        M.WU32(0x1EABC3CU, v1);
        L5: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((8)) * 4)))) != 0)) goto L9;
        v4 = ((uint)(((uint)(v1 + (uint)(((1)) * 1))) & 0xFFFFFFFEU));
        M.WU32(((uint)(v0 + (uint)(((8)) * 4))), v4);
        v5 = ((int)(M.U32(((uint)(v0 + (uint)(((7)) * 4))))));
        v2 = ((((uint)(((int)(((uint)((2))) * ((uint)(v5)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v6 = ((int)(((uint)((2))) * ((uint)(v5))));
        M.WU32(0x1EABC3CU, v4);
        M.WU32(0x17C69ACU, v4);
        if (!!((v2 != 0))) goto L11;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L13;
        Nat.Memset(v4, ((uint)((0))), ((uint)(v6)));
        M.WU32(0x1EABC3CU, ((uint)(v4 + (uint)((v6) * 1))));
        goto L14;
        L13: ;
        Nat.DB_LoadXFileData(v4, ((uint)(v6)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v6) * 1))));
        L14: ;
        L11: ;
        L9: ;
    }

    // sub_4BD1F0  void()
    void F_4BD1F0()
    {
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        Nat.Memset(M.U32(0x17C6B0CU), ((uint)((0))), ((uint)((52))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6B0CU), ((uint)((52))));
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((52)) * 1))));
    }

    // sub_4BD230  void __thiscall(void *this)
    void F_4BD230(uint l_this)
    {
        sbyte v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        v1 = ((sbyte)(M.U8(((uint)(M.U32(0x17C6D44U) + (uint)(((176)) * 1))))));
        if (!(((uint)(v1)) == ((uint)(((sbyte)(5)))))) goto L1;
        if (!(M.U32(((uint)(M.I32(0x17C6ED4U)))) != 0)) goto L3;
        v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v2);
        M.WU32(((uint)(M.I32(0x17C6ED4U))), v2);
        M.WU32(0x17C6EC8U, v2);
        F_4BD0E0();
        L3: ;
        goto L2;
        L1: ;
        if (!(((uint)(v1)) == ((uint)(((sbyte)(8)))))) goto L5;
        if (!(M.U32(((uint)(M.I32(0x17C6ED4U)))) != 0)) goto L7;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(M.I32(0x17C6ED4U))), v3);
        M.WU32(0x17C6B0CU, v3);
        F_4BD1F0();
        L7: ;
        goto L6;
        L5: ;
        if (!(M.U32(((uint)(M.I32(0x17C6ED4U)))) != 0)) goto L9;
        v4 = M.U32(0x1EABC3CU);
        M.WU32(((uint)(M.I32(0x17C6ED4U))), M.U32(0x1EABC3CU));
        M.WU32(0x17C6E18U, v4);
        F_4B6C20();
        L9: ;
        L6: ;
        L2: ;
    }

    // sub_4BD2B0  char **()
    uint F_4BD2B0()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        int v3 = 0;
        int v4 = 0;
        uint v5 = 0;
        int v6 = 0;
        int v7 = 0;
        uint v8 = 0;
        uint v9 = 0;
        int v10 = 0;
        uint v11 = 0;
        uint v12 = 0;
        int v13 = 0;
        uint v14 = 0;
        uint v15 = 0;
        int v16 = 0;
        uint result = 0;
        uint v18 = 0;
        v0 = M.U32(0x17C6D44U);
        v1 = M.U32(0x1EABC3CU);
        if (!(M.U32(((uint)(M.U32(0x17C6D44U) + (uint)(((45)) * 4)))) != 0)) goto L1;
        v2 = ((int)(M.U8(((uint)(M.U32(0x17C6D44U) + (uint)(((178)) * 1))))));
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v3 = ((int)(((uint)((32))) * ((uint)(((int)(((uint)(((int)(((uint)((3))) * ((uint)(v2)))))) + ((uint)((3)))))))));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(M.U32(0x17C6D44U) + (uint)(((45)) * 4))), v1);
        M.WI32(0x17C6F54U, ((int)(v1)));
        if (!(v3 != 0)) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(v1, ((uint)((0))), ((uint)(((int)(((uint)((32))) * ((uint)(((int)(((uint)(((int)(((uint)((3))) * ((uint)(v2)))))) + ((uint)((3))))))))))));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v1, ((uint)(((int)(((uint)((32))) * ((uint)(((int)(((uint)(((int)(((uint)((3))) * ((uint)(v2)))))) + ((uint)((3))))))))))));
        v0 = M.U32(0x17C6D44U);
        v1 = M.U32(0x1EABC3CU);
        L6: ;
        v1 = ((uint)(v1 + (uint)((v3) * 1)));
        M.WU32(0x1EABC3CU, v1);
        L3: ;
        L1: ;
        if (!(M.U32(((uint)(v0 + (uint)(((46)) * 4)))) != 0)) goto L7;
        v4 = ((int)(M.U8(((uint)(v0 + (uint)(((179)) * 1))))));
        v5 = ((uint)(((uint)(v1 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v6 = ((int)(((uint)((16))) * ((uint)(((int)(((uint)(((int)(((uint)((3))) * ((uint)(v4)))))) + ((uint)((3)))))))));
        M.WU32(0x1EABC3CU, v5);
        M.WU32(((uint)(v0 + (uint)(((46)) * 4))), v5);
        M.WI32(0x17C6F7CU, ((int)(v5)));
        if (!(v6 != 0)) goto L9;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L11;
        Nat.Memset(v5, ((uint)((0))), ((uint)(((int)(((uint)((16))) * ((uint)(((int)(((uint)(((int)(((uint)((3))) * ((uint)(v4)))))) + ((uint)((3))))))))))));
        goto L12;
        L11: ;
        Nat.DB_LoadXFileData(v5, ((uint)(((int)(((uint)((16))) * ((uint)(((int)(((uint)(((int)(((uint)((3))) * ((uint)(v4)))))) + ((uint)((3))))))))))));
        v0 = M.U32(0x17C6D44U);
        v5 = M.U32(0x1EABC3CU);
        L12: ;
        M.WU32(0x1EABC3CU, ((uint)(v5 + (uint)((v6) * 1))));
        L9: ;
        L7: ;
        M.WI32(0x17C6930U, ((int)(((uint)(v0 + (uint)(((47)) * 4))))));
        F_4BD050();
        v7 = ((int)(M.U32(((uint)(M.U32(0x17C6D44U) + (uint)(((54)) * 4))))));
        v8 = ((uint)(M.U32(0x17C6D44U) + (uint)(((216)) * 1)));
        M.WI32(0x17C6AF8U, ((int)(((uint)(((int)(M.U32(0x17C6D44U))))) + ((uint)((216))))));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6D44U) + (uint)(((216)) * 1))));
        if (!(v7 != 0)) goto L13;
        if (!(((uint)(v7)) == ((uint)((-1))))) goto L15;
        v9 = M.U32(0x1EABC3CU);
        M.WU32(v8, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v9)));
        Nat.Load_XStringData(v8);
        v8 = ((uint)(M.I32(0x17C6AF8U)));
        goto L16;
        L15: ;
        M.WU32(v8, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v7)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v7)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L16: ;
        L13: ;
        if (!(M.U32(v8) != 0)) goto L17;
        M.WU32(v8, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((30))), M.U32(v8)))))));
        L17: ;
        v10 = ((int)(M.U32(((uint)(M.U32(0x17C6D44U) + (uint)(((55)) * 4))))));
        v11 = ((uint)(M.U32(0x17C6D44U) + (uint)(((220)) * 1)));
        M.WI32(0x17C6AF8U, ((int)(((uint)(((int)(M.U32(0x17C6D44U))))) + ((uint)((220))))));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6D44U) + (uint)(((220)) * 1))));
        if (!(v10 != 0)) goto L19;
        if (!(((uint)(v10)) == ((uint)((-1))))) goto L21;
        v12 = M.U32(0x1EABC3CU);
        M.WU32(v11, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v12)));
        Nat.Load_XStringData(v11);
        v11 = ((uint)(M.I32(0x17C6AF8U)));
        goto L22;
        L21: ;
        M.WU32(v11, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v10)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v10)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L22: ;
        L19: ;
        if (!(M.U32(v11) != 0)) goto L23;
        M.WU32(v11, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((30))), M.U32(v11)))))));
        L23: ;
        v13 = ((int)(M.U32(((uint)(M.U32(0x17C6D44U) + (uint)(((56)) * 4))))));
        v14 = ((uint)(M.U32(0x17C6D44U) + (uint)(((224)) * 1)));
        M.WI32(0x17C6AF8U, ((int)(((uint)(((int)(M.U32(0x17C6D44U))))) + ((uint)((224))))));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6D44U) + (uint)(((224)) * 1))));
        if (!(v13 != 0)) goto L25;
        if (!(((uint)(v13)) == ((uint)((-1))))) goto L27;
        v15 = M.U32(0x1EABC3CU);
        M.WU32(v14, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v15)));
        Nat.Load_XStringData(v14);
        v14 = ((uint)(M.I32(0x17C6AF8U)));
        goto L28;
        L27: ;
        M.WU32(v14, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v13)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v13)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L28: ;
        L25: ;
        if (!(M.U32(v14) != 0)) goto L29;
        M.WU32(v14, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((30))), M.U32(v14)))))));
        L29: ;
        M.WI32(0x17C6ED4U, ((int)(((uint)(((int)(M.U32(0x17C6D44U))))) + ((uint)((244))))));
        F_4BD230(((uint)(M.U32(0x17C6D44U) + (uint)(((244)) * 1))));
        v16 = ((int)(M.U32(((uint)(M.U32(0x17C6D44U) + (uint)(((64)) * 4))))));
        result = ((uint)(M.U32(0x17C6D44U) + (uint)(((256)) * 1)));
        M.WI32(0x17C6FACU, ((int)(((uint)(((int)(M.U32(0x17C6D44U))))) + ((uint)((256))))));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6D44U) + (uint)(((256)) * 1))));
        if (!(v16 != 0)) goto L31;
        if (!(((uint)(v16)) == ((uint)((-1))))) goto L33;
        v18 = M.U32(0x1EABC3CU);
        M.WU32(result, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v18)));
        return ((uint)(Nat.Load_XStringData(result)));
        goto L34;
        L33: ;
        M.WU32(result, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v16)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v16)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L34: ;
        L31: ;
        return result;
        return {};
    }

    // sub_4BD560  void __cdecl(int a1)
    void F_4BD560(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((260))) * ((uint)(a1))));
        if (!(((int)(((uint)((260))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6D44U), ((uint)((0))), ((uint)(((int)(((uint)((260))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6D44U), ((uint)(((int)(((uint)((260))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6D44U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6D44U, v3);
        F_4BD2B0();
        v3 = ((uint)(v3 + (uint)(((260)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6D44U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4BD5E0  int()
    int F_4BD5E0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6BECU);
        M.WU32(M.U32(0x17C6BECU), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((4)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((5)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((6)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((7)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6BECU), ((uint)((32))));
        v0 = M.U32(0x17C6BECU);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((32)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6BECU);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((7)) * 4)))) != 0)) goto L7;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(v0 + (uint)(((7)) * 4))), v3);
        M.WU32(0x17C6D44U, v3);
        F_4BD560(((int)(((uint)(M.U32(((uint)(v0 + (uint)(((4)) * 4)))) + (uint)((((int)(((uint)(M.U32(((uint)(v0 + (uint)(((6)) * 4)))) + (uint)((((int)(M.U32(((uint)(v0 + (uint)(((5)) * 4))))))) * 1)))))) * 1))))));
        L7: ;
        return F_4CEE70();
        return {};
    }

    // sub_4BDAC0  void __usercall(int result@<eax>)
    void F_4BDAC0(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = result;
        v2 = ((int)(((uint)((36))) * ((uint)(result))));
        if (!(((int)(((uint)((36))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6968U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6968U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6968U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6968U, v3);
        M.WI32(0x17C6F08U, ((int)(((uint)(v3 + (uint)(((32)) * 1))))));
        F_4BB4D0();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6968U) + (uint)(((24)) * 1))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6968U) + (uint)(((28)) * 1))));
        F_4BA420();
        v3 = ((uint)(v3 + (uint)(((36)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6968U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4BDB50  void()
    void F_4BDB50()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        int v3 = 0;
        uint v4 = 0;
        int v5 = 0;
        uint v6 = 0;
        byte v7 = 0;
        int v8 = 0;
        uint v9 = 0;
        int v10 = 0;
        uint v11 = 0;
        int v12 = 0;
        uint v13 = 0;
        int v14 = 0;
        uint v15 = 0;
        uint v16 = 0;
        uint v17 = 0;
        int v18 = 0;
        uint v19 = 0;
        int v20 = 0;
        uint v21 = 0;
        int v22 = 0;
        int v23 = 0;
        int v24 = 0;
        uint v25 = 0;
        int v26 = 0;
        int v27 = 0;
        v0 = ((uint)(M.I32(0x17C6A7CU)));
        if (!(M.U32(((uint)(((int)(((uint)(M.I32(0x17C6A7CU))) + ((uint)((48)))))))) != 0)) goto L1;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C6A7CU))) + ((uint)((48))))))), v1);
        M.WU32(0x17C6968U, v1);
        F_4BDAC0(M.I32(((uint)(v0 + (uint)(((2)) * 4)))));
        v0 = ((uint)(M.I32(0x17C6A7CU)));
        L1: ;
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((13)) * 4)))) != 0)) goto L3;
        v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v3 = ((int)(((uint)((32))) * ((uint)(M.I32(((uint)(v0 + (uint)(((3)) * 4))))))));
        M.WU32(0x1EABC3CU, v2);
        M.WI32(((uint)(v0 + (uint)(((13)) * 4))), ((int)(v2)));
        M.WI32(0x17C6DC8U, ((int)(v2)));
        if (!(v3 != 0)) goto L5;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L7;
        Nat.Memset(v2, ((uint)((0))), ((uint)(v3)));
        goto L8;
        L7: ;
        Nat.DB_LoadXFileData(v2, ((uint)(v3)));
        v0 = ((uint)(M.I32(0x17C6A7CU)));
        v2 = M.U32(0x1EABC3CU);
        L8: ;
        M.WU32(0x1EABC3CU, ((uint)(v2 + (uint)((v3) * 1))));
        L5: ;
        L3: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((14)) * 4)))) != 0)) goto L9;
        v4 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v5 = ((int)(((uint)((32))) * ((uint)(M.I32(((uint)(v0 + (uint)(((3)) * 4))))))));
        M.WU32(0x1EABC3CU, v4);
        M.WI32(((uint)(v0 + (uint)(((14)) * 4))), ((int)(v4)));
        M.WI32(0x17C6B78U, ((int)(v4)));
        if (!(v5 != 0)) goto L11;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L13;
        Nat.Memset(v4, ((uint)((0))), ((uint)(v5)));
        goto L14;
        L13: ;
        Nat.DB_LoadXFileData(v4, ((uint)(v5)));
        v0 = ((uint)(M.I32(0x17C6A7CU)));
        v4 = M.U32(0x1EABC3CU);
        L14: ;
        M.WU32(0x1EABC3CU, ((uint)(v4 + (uint)((v5) * 1))));
        L11: ;
        L9: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((15)) * 4)))) != 0)) goto L15;
        v6 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v7 = ((((uint)(((int)(((uint)((36))) * ((uint)(M.I32(((uint)(v0 + (uint)(((3)) * 4)))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v8 = ((int)(((uint)((36))) * ((uint)(M.I32(((uint)(v0 + (uint)(((3)) * 4))))))));
        M.WU32(0x1EABC3CU, v6);
        M.WI32(((uint)(v0 + (uint)(((15)) * 4))), ((int)(v6)));
        M.WI32(0x17C6928U, ((int)(v6)));
        if (!!((v7 != 0))) goto L17;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L19;
        Nat.Memset(v6, ((uint)((0))), ((uint)(v8)));
        goto L20;
        L19: ;
        Nat.DB_LoadXFileData(v6, ((uint)(v8)));
        v0 = ((uint)(M.I32(0x17C6A7CU)));
        v6 = M.U32(0x1EABC3CU);
        L20: ;
        M.WU32(0x1EABC3CU, ((uint)(v6 + (uint)((v8) * 1))));
        L17: ;
        L15: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((16)) * 4)))) != 0)) goto L21;
        v9 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v7 = ((((uint)(((int)(((uint)((4))) * ((uint)(M.I32(((uint)(v0 + (uint)(((9)) * 4)))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v10 = ((int)(((uint)((4))) * ((uint)(M.I32(((uint)(v0 + (uint)(((9)) * 4))))))));
        M.WU32(0x1EABC3CU, v9);
        M.WI32(((uint)(v0 + (uint)(((16)) * 4))), ((int)(v9)));
        M.WI32(0x17C6D34U, ((int)(v9)));
        if (!!((v7 != 0))) goto L23;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L25;
        Nat.Memset(v9, ((uint)((0))), ((uint)(v10)));
        goto L26;
        L25: ;
        Nat.DB_LoadXFileData(v9, ((uint)(v10)));
        v0 = ((uint)(M.I32(0x17C6A7CU)));
        v9 = M.U32(0x1EABC3CU);
        L26: ;
        M.WU32(0x1EABC3CU, ((uint)(v9 + (uint)((v10) * 1))));
        L23: ;
        L21: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((17)) * 4)))) != 0)) goto L27;
        v11 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v7 = ((((uint)(((int)(((uint)((4))) * ((uint)(M.I32(((uint)(v0 + (uint)(((4)) * 4)))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v12 = ((int)(((uint)((4))) * ((uint)(M.I32(((uint)(v0 + (uint)(((4)) * 4))))))));
        M.WU32(0x1EABC3CU, v11);
        M.WI32(((uint)(v0 + (uint)(((17)) * 4))), ((int)(v11)));
        M.WI32(0x17C6C80U, ((int)(v11)));
        if (!!((v7 != 0))) goto L29;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L31;
        Nat.Memset(v11, ((uint)((0))), ((uint)(v12)));
        goto L32;
        L31: ;
        Nat.DB_LoadXFileData(v11, ((uint)(v12)));
        v0 = ((uint)(M.I32(0x17C6A7CU)));
        v11 = M.U32(0x1EABC3CU);
        L32: ;
        M.WU32(0x1EABC3CU, ((uint)(v11 + (uint)((v12) * 1))));
        L29: ;
        L27: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((18)) * 4)))) != 0)) goto L33;
        v13 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v7 = ((((uint)(((int)(((uint)((4))) * ((uint)(((int)(((uint)(M.I32(((uint)(v0 + (uint)(((4)) * 4)))))) * ((uint)(M.I32(((uint)(v0 + (uint)(((6)) * 4)))))))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v14 = ((int)(((uint)((4))) * ((uint)(((int)(((uint)(M.I32(((uint)(v0 + (uint)(((4)) * 4)))))) * ((uint)(M.I32(((uint)(v0 + (uint)(((6)) * 4))))))))))));
        M.WU32(0x1EABC3CU, v13);
        M.WI32(((uint)(v0 + (uint)(((18)) * 4))), ((int)(v13)));
        M.WI32(0x17C6C80U, ((int)(v13)));
        if (!!((v7 != 0))) goto L35;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L37;
        Nat.Memset(v13, ((uint)((0))), ((uint)(v14)));
        goto L38;
        L37: ;
        Nat.DB_LoadXFileData(v13, ((uint)(v14)));
        v0 = ((uint)(M.I32(0x17C6A7CU)));
        v13 = M.U32(0x1EABC3CU);
        L38: ;
        M.WU32(0x1EABC3CU, ((uint)(v13 + (uint)((v14) * 1))));
        L35: ;
        L33: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((19)) * 4)))) != 0)) goto L39;
        v15 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((15))))) & 0xFFFFFFF0U));
        v16 = ((uint)(((uint)(((int)(((uint)(M.I32(((uint)(v0 + (uint)(((3)) * 4)))))) + ((uint)((15))))))) & 0xFFFFFFF0U));
        M.WU32(0x1EABC3CU, v15);
        M.WI32(((uint)(v0 + (uint)(((19)) * 4))), ((int)(v15)));
        M.WI32(0x17C6E44U, ((int)(v15)));
        if (!(v16 != 0)) goto L41;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L43;
        Nat.Memset(v15, ((uint)((0))), v16);
        goto L44;
        L43: ;
        Nat.DB_LoadXFileData(v15, v16);
        v0 = ((uint)(M.I32(0x17C6A7CU)));
        v15 = M.U32(0x1EABC3CU);
        L44: ;
        M.WU32(0x1EABC3CU, ((uint)(v15 + (uint)((((int)(v16))) * 1))));
        L41: ;
        L39: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((20)) * 4)))) != 0)) goto L45;
        v17 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v7 = ((((uint)(((int)(((uint)((12))) * ((uint)(M.I32(((uint)(v0 + (uint)(((3)) * 4)))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v18 = ((int)(((uint)((12))) * ((uint)(M.I32(((uint)(v0 + (uint)(((3)) * 4))))))));
        M.WU32(0x1EABC3CU, v17);
        M.WI32(((uint)(v0 + (uint)(((20)) * 4))), ((int)(v17)));
        M.WI32(0x17C6F14U, ((int)(v17)));
        if (!!((v7 != 0))) goto L47;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L49;
        Nat.Memset(v17, ((uint)((0))), ((uint)(v18)));
        goto L50;
        L49: ;
        Nat.DB_LoadXFileData(v17, ((uint)(v18)));
        v0 = ((uint)(M.I32(0x17C6A7CU)));
        v17 = M.U32(0x1EABC3CU);
        L50: ;
        M.WU32(0x1EABC3CU, ((uint)(v17 + (uint)((v18) * 1))));
        L47: ;
        L45: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((21)) * 4)))) != 0)) goto L51;
        v19 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((15))))) & 0xFFFFFFF0U));
        v7 = ((((uint)(((uint)((4))) * ((uint)(((uint)(((int)(((uint)(M.I32(((uint)(v0 + (uint)(((3)) * 4)))))) + ((uint)((3))))))) & 0xFFFFFFFCU)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v20 = ((int)(((uint)(((uint)((4))) * ((uint)(((uint)(((int)(((uint)(M.I32(((uint)(v0 + (uint)(((3)) * 4)))))) + ((uint)((3))))))) & 0xFFFFFFFCU))))));
        M.WU32(0x1EABC3CU, v19);
        M.WI32(((uint)(v0 + (uint)(((21)) * 4))), ((int)(v19)));
        M.WI32(0x17C6C90U, ((int)(v19)));
        if (!!((v7 != 0))) goto L53;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L55;
        Nat.Memset(v19, ((uint)((0))), ((uint)(v20)));
        goto L56;
        L55: ;
        Nat.DB_LoadXFileData(v19, ((uint)(v20)));
        v0 = ((uint)(M.I32(0x17C6A7CU)));
        v19 = M.U32(0x1EABC3CU);
        L56: ;
        M.WU32(0x1EABC3CU, ((uint)(v19 + (uint)((v20) * 1))));
        L53: ;
        L51: ;
        F_4CEE70();
        v21 = M.U32(0x1EABC3CU);
        if (!(M.I32(((uint)(v0 + (uint)(((22)) * 4)))) != 0)) goto L57;
        v22 = M.I32(((uint)(v0 + (uint)(((5)) * 4))));
        v21 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((1))))) & 0xFFFFFFFEU));
        v7 = ((((uint)(((int)(((uint)((2))) * ((uint)(v22)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v23 = ((int)(((uint)((2))) * ((uint)(v22))));
        M.WU32(0x1EABC3CU, v21);
        M.WI32(((uint)(v0 + (uint)(((22)) * 4))), ((int)(v21)));
        M.WU32(0x17C6A90U, v21);
        if (!!((v7 != 0))) goto L59;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L61;
        Nat.Memset(v21, ((uint)((0))), ((uint)(v23)));
        goto L62;
        L61: ;
        Nat.DB_LoadXFileData(v21, ((uint)(v23)));
        v0 = ((uint)(M.I32(0x17C6A7CU)));
        v21 = M.U32(0x1EABC3CU);
        L62: ;
        v21 = ((uint)(v21 + (uint)((v23) * 1)));
        M.WU32(0x1EABC3CU, v21);
        L59: ;
        L57: ;
        if (!(M.I32(((uint)(v0 + (uint)(((23)) * 4)))) != 0)) goto L63;
        v24 = ((int)(((uint)((52))) * ((uint)(M.I32(((uint)(v0 + (uint)(((5)) * 4))))))));
        v21 = ((uint)(((uint)(v21 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v21);
        M.WI32(((uint)(v0 + (uint)(((23)) * 4))), ((int)(v21)));
        M.WI32(0x17C6FA4U, ((int)(v21)));
        if (!(v24 != 0)) goto L65;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L67;
        Nat.Memset(v21, ((uint)((0))), ((uint)(v24)));
        goto L68;
        L67: ;
        Nat.DB_LoadXFileData(v21, ((uint)(v24)));
        v0 = ((uint)(M.I32(0x17C6A7CU)));
        v21 = M.U32(0x1EABC3CU);
        L68: ;
        v21 = ((uint)(v21 + (uint)((v24) * 1)));
        M.WU32(0x1EABC3CU, v21);
        L65: ;
        L63: ;
        if (!(M.I32(((uint)(v0 + (uint)(((24)) * 4)))) != 0)) goto L69;
        v25 = ((uint)(((uint)(v21 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WI32(((uint)(v0 + (uint)(((24)) * 4))), ((int)(v25)));
        v26 = ((int)(((uint)((2))) * ((uint)(M.I32(((uint)(v0 + (uint)(((11)) * 4))))))));
        v7 = ((((uint)(((int)(((uint)((2))) * ((uint)(v26)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v27 = ((int)(((uint)((2))) * ((uint)(v26))));
        M.WU32(0x1EABC3CU, v25);
        M.WI32(0x17C69C4U, ((int)(v25)));
        if (!!((v7 != 0))) goto L71;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L73;
        Nat.Memset(v25, ((uint)((0))), ((uint)(v27)));
        M.WU32(0x1EABC3CU, ((uint)(v25 + (uint)((v27) * 1))));
        goto L74;
        L73: ;
        Nat.DB_LoadXFileData(v25, ((uint)(v27)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v27) * 1))));
        L74: ;
        L71: ;
        L69: ;
    }

    // sub_4BE220  int()
    int F_4BE220()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        int v3 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6B58U);
        Nat.Memset(M.U32(0x17C6B58U), ((uint)((0))), ((uint)((116))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6B58U), ((uint)((116))));
        v0 = M.U32(0x17C6B58U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((116)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!!((v1 != 0))) goto L3;
        goto LBL_9;
        L3: ;
        if (!(v1 != ((uint)((-1))))) goto L5;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        LBL_9: ;
        M.WI32(0x17C6A7CU, ((int)(((uint)(v0 + (uint)(((1)) * 4))))));
        F_4BDB50();
        return F_4CEE70();
        L5: ;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        M.WI32(0x17C6A7CU, ((int)(((uint)(M.U32(0x17C6B58U) + (uint)(((1)) * 4))))));
        F_4BDB50();
        return F_4CEE70();
        return {};
    }

    // sub_4BE2E0  int()
    int F_4BE2E0()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6C24U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6C24U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6C24U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6C24U))), v2);
            M.WU32(0x17C6B58U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4BE220();
            v4 = ((uint)(M.I32(0x17C6C24U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6C24U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((20))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6C24U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4BE440  void __usercall(int result@<eax>)
    void F_4BE440(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = result;
        v2 = ((int)(((uint)((92))) * ((uint)(result))));
        if (!(((int)(((uint)((92))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6CE0U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6CE0U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6CE0U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6CE0U, v3);
        M.WI32(0x17C69D4U, ((int)(((uint)(v3 + (uint)(((32)) * 1))))));
        F_4BBEF0();
        M.WI32(0x17C6B88U, ((int)(((uint)(((int)(M.U32(0x17C6CE0U))))) + ((uint)((40))))));
        F_4BCDF0();
        M.WI32(0x17C6F08U, ((int)(((uint)(((int)(M.U32(0x17C6CE0U))))) + ((uint)((44))))));
        F_4BB4D0();
        v3 = ((uint)(v3 + (uint)(((92)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6CE0U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4BE4D0  void __usercall(unsigned int result@<eax>)
    void F_4BE4D0(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((20))) * result))));
        if (!(((uint)(((uint)((20))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6BA4U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6BA4U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4BE510  void __usercall(unsigned int result@<eax>)
    void F_4BE510(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((32))) * result))));
        if (!(((uint)(((uint)((32))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6F0CU), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6F0CU), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4BE550  void __usercall(unsigned int result@<eax>)
    void F_4BE550(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((12))) * result))));
        if (!(((uint)(((uint)((12))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6D60U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6D60U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4BE6F0  void()
    void F_4BE6F0()
    {
        uint v0 = 0;
        uint v1 = 0;
        byte v2 = 0;
        int v3 = 0;
        int v4 = 0;
        uint v5 = 0;
        int v6 = 0;
        int v7 = 0;
        v0 = M.U32(0x1EABC3CU);
        v1 = ((uint)(M.I32(0x17C6ACCU)));
        if (!(M.U32(((uint)(((int)(((uint)(M.I32(0x17C6ACCU))) + ((uint)((4)))))))) != 0)) goto L1;
        v0 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v2 = ((((uint)(((int)(((uint)((8))) * M.U32(((uint)(M.I32(0x17C6ACCU)))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v3 = ((int)(((uint)((8))) * M.U32(((uint)(M.I32(0x17C6ACCU))))));
        M.WU32(0x1EABC3CU, v0);
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C6ACCU))) + ((uint)((4))))))), v0);
        M.WI32(0x17C6CA0U, ((int)(v0)));
        if (!!((v2 != 0))) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(v0, ((uint)((0))), ((uint)(v3)));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v0, ((uint)(v3)));
        v1 = ((uint)(M.I32(0x17C6ACCU)));
        v0 = M.U32(0x1EABC3CU);
        L6: ;
        v0 = ((uint)(v0 + (uint)((v3) * 1)));
        M.WU32(0x1EABC3CU, v0);
        L3: ;
        L1: ;
        if (!(M.U32(((uint)(v1 + (uint)(((3)) * 4)))) != 0)) goto L7;
        v0 = ((uint)(((uint)(v0 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v4 = ((int)(((uint)((32))) * M.U32(((uint)(v1 + (uint)(((2)) * 4))))));
        M.WU32(0x1EABC3CU, v0);
        M.WU32(((uint)(v1 + (uint)(((3)) * 4))), v0);
        M.WI32(0x17C6AD0U, ((int)(v0)));
        if (!(v4 != 0)) goto L9;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L11;
        Nat.Memset(v0, ((uint)((0))), ((uint)(v4)));
        goto L12;
        L11: ;
        Nat.DB_LoadXFileData(v0, ((uint)(v4)));
        v1 = ((uint)(M.I32(0x17C6ACCU)));
        v0 = M.U32(0x1EABC3CU);
        L12: ;
        v0 = ((uint)(v0 + (uint)((v4) * 1)));
        M.WU32(0x1EABC3CU, v0);
        L9: ;
        L7: ;
        if (!(M.U32(((uint)(v1 + (uint)(((5)) * 4)))) != 0)) goto L13;
        v5 = ((uint)(((uint)(v0 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WU32(((uint)(v1 + (uint)(((5)) * 4))), v5);
        v6 = ((int)(((uint)((10))) * M.U32(((uint)(v1 + (uint)(((4)) * 4))))));
        v2 = ((((uint)(((int)(((uint)((2))) * ((uint)(v6)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v7 = ((int)(((uint)((2))) * ((uint)(v6))));
        M.WU32(0x1EABC3CU, v5);
        M.WI32(0x17C6EF0U, ((int)(v5)));
        if (!!((v2 != 0))) goto L15;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L17;
        Nat.Memset(v5, ((uint)((0))), ((uint)(v7)));
        M.WU32(0x1EABC3CU, ((uint)(v5 + (uint)((v7) * 1))));
        goto L18;
        L17: ;
        Nat.DB_LoadXFileData(v5, ((uint)(v7)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v7) * 1))));
        L18: ;
        L15: ;
        L13: ;
    }

    // sub_4BE820  void __usercall(int result@<eax>)
    void F_4BE820(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        v1 = result;
        v2 = ((int)(((uint)((20))) * ((uint)(result))));
        if (!(((int)(((uint)((20))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6958U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6958U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6958U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = M.U32(v3);
        M.WU32(0x17C6958U, v3);
        M.WU32(0x17C6EECU, v3);
        if (!(v4 != 0)) goto L10;
        if (!(v4 == ((uint)((-1))))) goto L12;
        v5 = M.U32(0x1EABC3CU);
        M.WU32(v3, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v5)));
        Nat.Load_XStringData(v3);
        goto L13;
        L12: ;
        M.WU32(v3, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v4 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v4 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L13: ;
        L10: ;
        v3 = ((uint)(v3 + (uint)(((5)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6958U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4BE8C0  int()
    int F_4BE8C0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        uint v5 = 0;
        uint v6 = 0;
        int v7 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6F38U);
        Nat.Memset(M.U32(0x17C6F38U), ((uint)((0))), ((uint)((44))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6F38U), ((uint)((44))));
        v0 = M.U32(0x17C6F38U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((44)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6F38U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((1)) * 4)))) != 0)) goto L7;
        v3 = M.U32(0x1EABC3CU);
        v4 = ((int)(M.U32(((uint)(v0 + (uint)(((2)) * 4))))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), M.U32(0x1EABC3CU));
        M.WI32(0x17C6B84U, ((int)(v3)));
        if (!(v4 != 0)) goto L9;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L11;
        Nat.Memset(v3, ((uint)((0))), ((uint)(v4)));
        goto L12;
        L11: ;
        Nat.DB_LoadXFileData(v3, ((uint)(v4)));
        v0 = M.U32(0x17C6F38U);
        v3 = M.U32(0x1EABC3CU);
        L12: ;
        M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)((v4) * 1))));
        L9: ;
        L7: ;
        M.WI32(0x17C6ACCU, ((int)(((uint)(v0 + (uint)(((3)) * 4))))));
        F_4BE6F0();
        v5 = M.U32(0x17C6F38U);
        if (!(M.U32(((uint)(M.U32(0x17C6F38U) + (uint)(((9)) * 4)))) != 0)) goto L13;
        v6 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v6);
        M.WU32(((uint)(M.U32(0x17C6F38U) + (uint)(((9)) * 4))), v6);
        M.WU32(0x17C6958U, v6);
        F_4BE820(((int)(M.U8(((uint)(v5 + (uint)(((40)) * 1)))))));
        L13: ;
        return F_4CEE70();
        return {};
    }

    // sub_4BE9F0  int()
    int F_4BE9F0()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6F10U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6F10U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6F10U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6F10U))), v2);
            M.WU32(0x17C6F38U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4BE8C0();
            v4 = ((uint)(M.I32(0x17C6F10U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6F10U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((19))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6F10U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4BEAE0  int()
    int F_4BEAE0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        int v5 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6C2CU);
        M.WU32(M.U32(0x17C6C2CU), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((4)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((5)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((6)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((7)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((8)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6C2CU), ((uint)((36))));
        v0 = M.U32(0x17C6C2CU);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((36)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6C2CU);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((1)) * 4)))) != 0)) goto L7;
        v3 = M.U32(0x1EABC3CU);
        v4 = ((int)(M.U32(((uint)(v0 + (uint)(((2)) * 4))))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), M.U32(0x1EABC3CU));
        M.WI32(0x17C6B84U, ((int)(v3)));
        if (!(v4 != 0)) goto L9;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L11;
        Nat.Memset(v3, ((uint)((0))), ((uint)(v4)));
        goto L12;
        L11: ;
        Nat.DB_LoadXFileData(v3, ((uint)(v4)));
        v0 = M.U32(0x17C6C2CU);
        v3 = M.U32(0x1EABC3CU);
        L12: ;
        M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)((v4) * 1))));
        L9: ;
        L7: ;
        M.WI32(0x17C6ACCU, ((int)(((uint)(v0 + (uint)(((3)) * 4))))));
        F_4BE6F0();
        return F_4CEE70();
        return {};
    }

    // sub_4BEBF0  int()
    int F_4BEBF0()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6E7CU))))));
            if (!(M.U32(((uint)(M.I32(0x17C6E7CU)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6E7CU))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6E7CU))), v2);
            M.WU32(0x17C6C2CU, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4BEAE0();
            v4 = ((uint)(M.I32(0x17C6E7CU)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6E7CU)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((42))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6E7CU)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4BED80  void __usercall(unsigned int result@<eax>)
    void F_4BED80(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((28))) * result))));
        if (!(((uint)(((uint)((28))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6A3CU), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A3CU), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4BEDD0  void __cdecl(int a1)
    void F_4BEDD0(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((76))) * ((uint)(a1))));
        if (!(((int)(((uint)((76))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6DB0U), ((uint)((0))), ((uint)(((int)(((uint)((76))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6DB0U), ((uint)(((int)(((uint)((76))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6DB0U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6DB0U, v3);
        M.WI32(0x17C69D4U, ((int)(v3)));
        F_4BBEF0();
        v3 = ((uint)(v3 + (uint)(((76)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6DB0U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4BEE50  void *()
    uint F_4BEE50()
    {
        uint v0 = 0;
        byte v1 = 0;
        uint result = 0;
        if (!(M.U32(M.U32(0x17C6E1CU)) == ((uint)((-1))))) goto L1;
        v0 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v1 = ((((uint)(M.I32(0x1EBDC44U))) == ((uint)((2)))) ? (byte)1 : (byte)0);
        M.WU32(0x1EABC3CU, v0);
        M.WU32(M.U32(0x17C6E1CU), v0);
        M.WU32(0x17C6B54U, v0);
        if (!(v1 != 0)) goto L3;
        M.WU32(v0, ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((4)) * 4))), ((uint)((0))));
        result = ((uint)(v0 + (uint)(((5)) * 4)));
        M.WU32(0x1EABC3CU, result);
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(v0, ((uint)((20))));
        result = ((uint)(M.U32(0x1EABC3CU) + (uint)(((20)) * 1)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((20)) * 1))));
        L4: ;
        goto L2;
        L1: ;
        result = ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(M.U32(M.U32(0x17C6E1CU)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(M.U32(M.U32(0x17C6E1CU)) - ((uint)((1))))))) & ((uint)((268435455)))))))))));
        M.WU32(M.U32(0x17C6E1CU), result);
        L2: ;
        return result;
        return {};
    }

    // sub_4BEED0  void __usercall(int result@<eax>)
    void F_4BEED0(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        byte v4 = 0;
        v1 = result;
        v2 = ((int)(((uint)((8))) * ((uint)(result))));
        if (!(((int)(((uint)((8))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6E1CU), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6E1CU), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6E1CU);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = ((M.U32(v3) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6E1CU, v3);
        if (!!((v4 != 0))) goto L10;
        F_4BEE50();
        L10: ;
        v3 = ((uint)(v3 + (uint)(((2)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6E1CU, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4BEF40  void __usercall(unsigned int result@<eax>)
    void F_4BEF40(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((40))) * result))));
        if (!(((uint)(((uint)((40))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6AF0U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6AF0U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4BEF80  void()
    void F_4BEF80()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        byte v3 = 0;
        int v4 = 0;
        if (!(M.U32(((uint)(M.I32(0x17C6B38U)))) == ((uint)((-1))))) goto L1;
        v0 = M.U32(0x17C6DD0U);
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((1))))) & 0xFFFFFFFEU));
        M.WU32(((uint)(M.I32(0x17C6B38U))), v1);
        v2 = ((int)(M.I16(((uint)(v0 + (uint)(((1)) * 2))))));
        v3 = ((((uint)(((int)(((uint)((2))) * ((uint)(v2)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v4 = ((int)(((uint)((2))) * ((uint)(v2))));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(0x17C6ABCU, v1);
        if (!!((v3 != 0))) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(v1, ((uint)((0))), ((uint)(v4)));
        M.WU32(0x1EABC3CU, ((uint)(v1 + (uint)((v4) * 1))));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v1, ((uint)(v4)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v4) * 1))));
        L6: ;
        L3: ;
        goto L2;
        L1: ;
        M.WU32(((uint)(M.I32(0x17C6B38U))), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(M.U32(((uint)(M.I32(0x17C6B38U)))) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(M.U32(((uint)(M.I32(0x17C6B38U)))) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L2: ;
    }

    // sub_4BF010  void __cdecl(int a1)
    void F_4BF010(int a1)
    {
        int v1 = 0;
        byte v2 = 0;
        int v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        byte v6 = 0;
        int v7 = 0;
        v1 = a1;
        v2 = ((((uint)(((int)(((uint)((20))) * ((uint)(a1)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v3 = ((int)(((uint)((20))) * ((uint)(a1))));
        v7 = ((int)(((uint)((20))) * ((uint)(a1))));
        if (!!((v2 != 0))) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6DD0U), ((uint)((0))), ((uint)(v3)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6DD0U), ((uint)(v3)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v3) * 1))));
        L1: ;
        v4 = M.U32(0x17C6DD0U);
        if (!(v1 > (0))) goto L5;
        v5 = ((uint)(M.U32(0x17C6DD0U) + (uint)(((8)) * 1)));
        L7: ;
        v6 = ((((int)(M.U16(((uint)(v5 - (uint)(((3)) * 2)))))) <= (0)) ? (byte)1 : (byte)0);
        M.WU32(0x17C6DD0U, v4);
        M.WI32(0x17C6D48U, ((int)(v5)));
        if (!!((v6 != 0))) goto L10;
        v2 = ((M.U32(v5) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WI32(0x17C6B38U, ((int)(v5)));
        if (!!((v2 != 0))) goto L12;
        F_4BEF80();
        L12: ;
        L10: ;
        v4 = ((uint)(v4 + (uint)(((20)) * 1)));
        v5 = ((uint)(v5 + (uint)(((5)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        v3 = v7;
        L5: ;
        M.WU32(0x17C6DD0U, ((uint)(v4 + (uint)((((int)(0 - v3))) * 1))));
    }

    // sub_4BF0A0  void __usercall(unsigned int result@<eax>)
    void F_4BF0A0(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((28))) * result))));
        if (!(((uint)(((uint)((28))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6F4CU), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6F4CU), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4BF0F0  char *__usercall@<eax>(int a1@<eax>)
    uint F_4BF0F0(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint result = 0;
        uint v4 = 0;
        int v5 = 0;
        uint v6 = 0;
        byte v7 = 0;
        int v8 = 0;
        v2 = ((int)(((uint)((12))) * ((uint)(a1))));
        if (!(((int)(((uint)((12))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6A54U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A54U), ((uint)(v2)));
        L4: ;
        result = ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        goto L2;
        L1: ;
        result = M.U32(0x1EABC3CU);
        L2: ;
        v4 = M.U32(0x17C6A54U);
        if (!(a1 > (0))) goto L5;
        v8 = a1;
        L7: ;
        v5 = ((int)(M.U32(((uint)(v4 + (uint)(((2)) * 4))))));
        M.WU32(0x17C6A54U, v4);
        if (!(v5 != 0)) goto L10;
        if (!(((uint)(v5)) == ((uint)((-1))))) goto L12;
        v6 = ((uint)(((uint)(result + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v7 = ((((uint)(M.I32(0x1EBDC44U))) == ((uint)((2)))) ? (byte)1 : (byte)0);
        M.WU32(0x1EABC3CU, v6);
        M.WU32(((uint)(v4 + (uint)(((2)) * 4))), v6);
        M.WU32(0x17C6F4CU, v6);
        if (!(v7 != 0)) goto L14;
        M.WU32(v6, ((uint)((0))));
        M.WU32(((uint)(v6 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v6 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v6 + (uint)(((3)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v6 + (uint)(((4)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v6 + (uint)(((5)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v6 + (uint)(((6)) * 4))), ((uint)((0))));
        result = ((uint)(v6 + (uint)(((28)) * 1)));
        M.WU32(0x1EABC3CU, result);
        goto L15;
        L14: ;
        Nat.DB_LoadXFileData(v6, ((uint)((28))));
        result = ((uint)(M.U32(0x1EABC3CU) + (uint)(((28)) * 1)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((28)) * 1))));
        L15: ;
        goto L13;
        L12: ;
        M.WU32(((uint)(v4 + (uint)(((2)) * 4))), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v5)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v5)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L13: ;
        L10: ;
        v4 = ((uint)(v4 + (uint)(((3)) * 4)));
        Rt::PreAdd(v8, -1);
        L8: ;
        if ((v8 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6A54U, ((uint)(v4 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
        return result;
        return {};
    }

    // sub_4BF1F0  void __usercall(unsigned int result@<eax>)
    void F_4BF1F0(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((32))) * result))));
        if (!(((uint)(((uint)((32))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6B50U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6B50U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4BF230  void __usercall(unsigned int result@<eax>)
    void F_4BF230(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((68))) * result))));
        if (!(((uint)(((uint)((68))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6A24U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A24U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4BF270  void __cdecl(int a1)
    void F_4BF270(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((36))) * ((uint)(a1))));
        if (!(((int)(((uint)((36))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6E98U), ((uint)((0))), ((uint)(((int)(((uint)((36))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6E98U), ((uint)(((int)(((uint)((36))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6E98U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6E98U, v3);
        M.WI32(0x17C6F1CU, ((int)(v3)));
        F_4BB1C0();
        v3 = ((uint)(v3 + (uint)(((36)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6E98U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4BF2F0  void __usercall(unsigned int result@<eax>)
    void F_4BF2F0(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((24))) * result))));
        if (!(((uint)(((uint)((24))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6AE8U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6AE8U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4BF330  void __usercall(unsigned int result@<eax>)
    void F_4BF330(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((2))) * result))));
        if (!(((uint)(((uint)((2))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6ABCU), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6ABCU), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4BF370  void __usercall(int result@<eax>)
    void F_4BF370(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        v1 = result;
        v2 = ((int)(((uint)((12))) * ((uint)(result))));
        if (!(((int)(((uint)((12))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C69E0U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C69E0U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C69E0U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = M.U32(v3);
        M.WU32(0x17C69E0U, v3);
        M.WU32(0x17C6EECU, v3);
        if (!(v4 != 0)) goto L10;
        if (!(v4 == ((uint)((-1))))) goto L12;
        v5 = M.U32(0x1EABC3CU);
        M.WU32(v3, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v5)));
        Nat.Load_XStringData(v3);
        goto L13;
        L12: ;
        M.WU32(v3, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v4 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v4 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L13: ;
        L10: ;
        v3 = ((uint)(v3 + (uint)(((3)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C69E0U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4BF410  int()
    int F_4BF410()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        uint v6 = 0;
        uint v7 = 0;
        uint v8 = 0;
        int v9 = 0;
        int v10 = 0;
        uint v11 = 0;
        uint v12 = 0;
        uint v13 = 0;
        uint v14 = 0;
        uint v15 = 0;
        uint v16 = 0;
        int v17 = 0;
        uint v18 = 0;
        int v19 = 0;
        uint v20 = 0;
        uint v21 = 0;
        uint v22 = 0;
        uint v23 = 0;
        uint v24 = 0;
        uint v25 = 0;
        int v26 = 0;
        uint v27 = 0;
        uint v28 = 0;
        uint v29 = 0;
        uint v30 = 0;
        uint v31 = 0;
        uint v32 = 0;
        uint v33 = 0;
        uint v34 = 0;
        int v35 = 0;
        uint v36 = 0;
        uint v37 = 0;
        uint v38 = 0;
        uint v39 = 0;
        uint v40 = 0;
        uint v41 = 0;
        uint v42 = 0;
        uint v43 = 0;
        uint v44 = 0;
        uint v45 = 0;
        uint v46 = 0;
        uint v47 = 0;
        int v48 = 0;
        int v49 = 0;
        int v50 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C68F8U);
        Nat.Memset(M.U32(0x17C68F8U), ((uint)((0))), ((uint)((256))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C68F8U), ((uint)((256))));
        v0 = M.U32(0x17C68F8U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((256)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C68F8U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        v3 = M.U32(((uint)(v0 + (uint)(((3)) * 4))));
        if (!(v3 != 0)) goto L7;
        if (!(v3 == ((uint)((-1))))) goto L9;
        v4 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v4);
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), v4);
        M.WU32(0x17C6B54U, v4);
        F_4BB090(M.U32(((uint)(v0 + (uint)(((2)) * 4)))));
        v0 = M.U32(0x17C68F8U);
        goto L10;
        L9: ;
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v3 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v3 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L10: ;
        L7: ;
        if (!(M.U32(((uint)(v0 + (uint)(((5)) * 4)))) != 0)) goto L11;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v5);
        M.WU32(((uint)(v0 + (uint)(((5)) * 4))), v5);
        M.WU32(0x17C6DB0U, v5);
        F_4BEDD0(((int)(M.U32(((uint)(v0 + (uint)(((4)) * 4)))))));
        v0 = M.U32(0x17C68F8U);
        L11: ;
        if (!(M.U32(((uint)(v0 + (uint)(((7)) * 4)))) != 0)) goto L13;
        v6 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v6);
        M.WU32(((uint)(v0 + (uint)(((7)) * 4))), v6);
        M.WU32(0x17C69E0U, v6);
        F_4BF370(((int)(M.U32(((uint)(v0 + (uint)(((6)) * 4)))))));
        v0 = M.U32(0x17C68F8U);
        L13: ;
        if (!(M.U32(((uint)(v0 + (uint)(((9)) * 4)))) != 0)) goto L15;
        v7 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v7);
        M.WU32(((uint)(v0 + (uint)(((9)) * 4))), v7);
        M.WU32(0x17C6EE8U, v7);
        F_4BB150(((int)(M.U32(((uint)(v0 + (uint)(((8)) * 4)))))));
        v0 = M.U32(0x17C68F8U);
        L15: ;
        v8 = M.U32(0x1EABC3CU);
        if (!(M.U32(((uint)(v0 + (uint)(((11)) * 4)))) != 0)) goto L17;
        v9 = ((int)(M.U32(((uint)(v0 + (uint)(((10)) * 4))))));
        M.WU32(((uint)(v0 + (uint)(((11)) * 4))), M.U32(0x1EABC3CU));
        M.WI32(0x17C6E9CU, ((int)(v8)));
        if (!(v9 != 0)) goto L19;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L21;
        Nat.Memset(v8, ((uint)((0))), ((uint)(v9)));
        goto L22;
        L21: ;
        Nat.DB_LoadXFileData(v8, ((uint)(v9)));
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L22: ;
        v8 = ((uint)(v8 + (uint)((v9) * 1)));
        M.WU32(0x1EABC3CU, v8);
        L19: ;
        L17: ;
        if (!(M.U32(((uint)(v0 + (uint)(((13)) * 4)))) != 0)) goto L23;
        v10 = ((int)(M.U32(((uint)(v0 + (uint)(((12)) * 4))))));
        v11 = ((uint)(((uint)(v8 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v11);
        M.WU32(((uint)(v0 + (uint)(((13)) * 4))), v11);
        M.WU32(0x17C6E1CU, v11);
        F_4BEED0(v10);
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L23: ;
        if (!(M.U32(((uint)(v0 + (uint)(((15)) * 4)))) != 0)) goto L25;
        v12 = M.U32(((uint)(v0 + (uint)(((14)) * 4))));
        v13 = ((uint)(((uint)(v8 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v13);
        M.WU32(((uint)(v0 + (uint)(((15)) * 4))), v13);
        M.WU32(0x17C6AF0U, v13);
        F_4BEF40(v12);
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L25: ;
        if (!(M.U32(((uint)(v0 + (uint)(((19)) * 4)))) != 0)) goto L27;
        v14 = M.U32(((uint)(v0 + (uint)(((18)) * 4))));
        v15 = ((uint)(((uint)(v8 + (uint)(((1)) * 1))) & 0xFFFFFFFEU));
        M.WU32(0x1EABC3CU, v15);
        M.WU32(((uint)(v0 + (uint)(((19)) * 4))), v15);
        M.WU32(0x17C6ABCU, v15);
        F_4BF330(v14);
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L27: ;
        if (!(M.U32(((uint)(v0 + (uint)(((17)) * 4)))) != 0)) goto L29;
        v16 = ((uint)(((uint)(v8 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v49 = ((int)(M.U32(((uint)(v0 + (uint)(((16)) * 4))))));
        M.WU32(0x1EABC3CU, v16);
        M.WU32(((uint)(v0 + (uint)(((17)) * 4))), v16);
        M.WU32(0x17C6DD0U, v16);
        F_4BF010(v49);
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L29: ;
        if (!(M.U32(((uint)(v0 + (uint)(((21)) * 4)))) != 0)) goto L31;
        v17 = ((int)(M.U32(((uint)(v0 + (uint)(((20)) * 4))))));
        v18 = ((uint)(((uint)(v8 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v18);
        M.WU32(((uint)(v0 + (uint)(((21)) * 4))), v18);
        M.WU32(0x17C693CU, v18);
        F_4B6CD0(v17);
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L31: ;
        if (!(M.U32(((uint)(v0 + (uint)(((23)) * 4)))) != 0)) goto L33;
        v19 = ((int)(M.U32(((uint)(v0 + (uint)(((22)) * 4))))));
        v20 = ((uint)(((uint)(v8 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v20);
        M.WU32(((uint)(v0 + (uint)(((23)) * 4))), v20);
        M.WU32(0x17C6BBCU, v20);
        F_4B6DD0(v19);
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L33: ;
        if (!(M.U32(((uint)(v0 + (uint)(((25)) * 4)))) != 0)) goto L35;
        M.WU32(((uint)(v0 + (uint)(((25)) * 4))), ((uint)(((uint)(v8 + (uint)(((1)) * 1))) & 0xFFFFFFFEU)));
        v21 = M.U32(((uint)(v0 + (uint)(((24)) * 4))));
        M.WU32(0x1EABC3CU, ((uint)(((uint)(v8 + (uint)(((1)) * 1))) & 0xFFFFFFFEU)));
        M.WU32(0x17C6A90U, ((uint)(((uint)(v8 + (uint)(((1)) * 1))) & 0xFFFFFFFEU)));
        F_4B6ED0(((uint)(((int)(((uint)((3))) * v21)))));
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L35: ;
        if (!(M.U32(((uint)(v0 + (uint)(((26)) * 4)))) != 0)) goto L37;
        v22 = M.U32(((uint)(v0 + (uint)(((24)) * 4))));
        v23 = ((uint)(((uint)(((int)(((int)(((uint)(((int)(((uint)((3))) * ((uint)(((int)(v22)))))))) + ((uint)((31))))) >> (3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v8);
        M.WU32(((uint)(v0 + (uint)(((26)) * 4))), v8);
        M.WU32(0x17C6AFCU, v8);
        if (!(v23 != 0)) goto L39;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L41;
        Nat.Memset(v8, ((uint)((0))), ((uint)(((uint)(((int)(((int)(((uint)(((int)(((uint)((3))) * ((uint)(((int)(v22)))))))) + ((uint)((31))))) >> (3))))) & 0xFFFFFFFCU)));
        goto L42;
        L41: ;
        Nat.DB_LoadXFileData(v8, ((uint)(((uint)(((int)(((int)(((uint)(((int)(((uint)((3))) * ((uint)(((int)(v22)))))))) + ((uint)((31))))) >> (3))))) & 0xFFFFFFFCU)));
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L42: ;
        v8 = ((uint)(v8 + (uint)((((int)(v23))) * 1)));
        M.WU32(0x1EABC3CU, v8);
        L39: ;
        L37: ;
        if (!(M.U32(((uint)(v0 + (uint)(((28)) * 4)))) != 0)) goto L43;
        v24 = M.U32(((uint)(v0 + (uint)(((27)) * 4))));
        v25 = ((uint)(((uint)(v8 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v25);
        M.WU32(((uint)(v0 + (uint)(((28)) * 4))), v25);
        M.WU32(0x17C6F4CU, v25);
        F_4BF0A0(v24);
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L43: ;
        if (!(M.U32(((uint)(v0 + (uint)(((30)) * 4)))) != 0)) goto L45;
        v26 = ((int)(M.U32(((uint)(v0 + (uint)(((29)) * 4))))));
        v27 = ((uint)(((uint)(v8 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v27);
        M.WU32(((uint)(v0 + (uint)(((30)) * 4))), v27);
        M.WU32(0x17C6A54U, v27);
        F_4BF0F0(v26);
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L45: ;
        if (!(M.U32(((uint)(v0 + (uint)(((32)) * 4)))) != 0)) goto L47;
        v28 = M.U32(((uint)(v0 + (uint)(((31)) * 4))));
        v29 = ((uint)(((uint)(v8 + (uint)(((15)) * 1))) & 0xFFFFFFF0U));
        M.WU32(0x1EABC3CU, v29);
        M.WU32(((uint)(v0 + (uint)(((32)) * 4))), v29);
        M.WU32(0x17C6B50U, v29);
        F_4BF1F0(v28);
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L47: ;
        if (!(M.U32(((uint)(v0 + (uint)(((34)) * 4)))) != 0)) goto L49;
        v30 = M.U32(((uint)(v0 + (uint)(((33)) * 4))));
        v31 = ((uint)(((uint)(v8 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v31);
        M.WU32(((uint)(v0 + (uint)(((34)) * 4))), v31);
        M.WU32(0x17C6A24U, v31);
        F_4BF230(v30);
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L49: ;
        if (!(M.U32(((uint)(v0 + (uint)(((36)) * 4)))) != 0)) goto L51;
        v32 = ((uint)(((uint)(v8 + (uint)(((127)) * 1))) & 0xFFFFFF80U));
        v50 = ((int)(M.U16(((uint)(v0 + (uint)(((70)) * 2))))));
        M.WU32(0x1EABC3CU, v32);
        M.WU32(((uint)(v0 + (uint)(((36)) * 4))), v32);
        M.WU32(0x17C6E98U, v32);
        F_4BF270(v50);
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L51: ;
        if (!(M.U32(((uint)(v0 + (uint)(((37)) * 4)))) != 0)) goto L53;
        v33 = ((uint)(M.U16(((uint)(v0 + (uint)(((70)) * 2))))));
        v34 = ((uint)(((uint)(v8 + (uint)(((127)) * 1))) & 0xFFFFFF80U));
        M.WU32(0x1EABC3CU, v34);
        M.WU32(((uint)(v0 + (uint)(((37)) * 4))), v34);
        M.WU32(0x17C6AE8U, v34);
        F_4BF2F0(v33);
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L53: ;
        if (!(M.U32(((uint)(v0 + (uint)(((38)) * 4)))) != 0)) goto L55;
        v35 = ((int)(M.U16(((uint)(v0 + (uint)(((70)) * 2))))));
        v36 = ((uint)(((uint)(v8 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v36);
        M.WU32(((uint)(v0 + (uint)(((38)) * 4))), v36);
        M.WU32(0x17C6CB8U, v36);
        F_4B6C90(v35);
        v0 = M.U32(0x17C68F8U);
        v8 = M.U32(0x1EABC3CU);
        L55: ;
        if (!(M.U32(((uint)(v0 + (uint)(((41)) * 4)))) != 0)) goto L57;
        v37 = ((uint)(M.U16(((uint)(v0 + (uint)(((80)) * 2))))));
        v38 = ((uint)(((uint)(v8 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v38);
        M.WU32(((uint)(v0 + (uint)(((41)) * 4))), v38);
        M.WU32(0x17C6A3CU, v38);
        F_4BED80(v37);
        v0 = M.U32(0x17C68F8U);
        L57: ;
        M.WI32(0x17C6F10U, ((int)(((uint)(v0 + (uint)(((39)) * 4))))));
        F_4BE9F0();
        v39 = M.U32(0x17C68F8U);
        if (!(M.U32(((uint)(M.U32(0x17C68F8U) + (uint)(((43)) * 4)))) != 0)) goto L59;
        v40 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v40);
        M.WU32(((uint)(M.U32(0x17C68F8U) + (uint)(((43)) * 4))), v40);
        M.WU32(0x17C6CE0U, v40);
        F_4BE440(((int)(M.U16(((uint)(v39 + (uint)(((84)) * 2)))))));
        v39 = M.U32(0x17C68F8U);
        L59: ;
        if (!(M.U32(((uint)(v39 + (uint)(((44)) * 4)))) != 0)) goto L61;
        v41 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v41);
        M.WU32(((uint)(v39 + (uint)(((44)) * 4))), v41);
        M.WU32(0x17C6CE0U, v41);
        F_4BE440(((int)(M.U16(((uint)(v39 + (uint)(((85)) * 2)))))));
        v39 = M.U32(0x17C68F8U);
        L61: ;
        F_4CEE10((2));
        if (!(M.U32(((uint)(v39 + (uint)(((45)) * 4)))) != 0)) goto L63;
        v42 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v42);
        M.WU32(((uint)(v39 + (uint)(((45)) * 4))), v42);
        M.WU32(0x17C6F0CU, v42);
        F_4BE510(((uint)(M.U16(((uint)(v39 + (uint)(((84)) * 2)))))));
        v39 = M.U32(0x17C68F8U);
        L63: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v39 + (uint)(((46)) * 4)))) != 0)) goto L65;
        v43 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v43);
        M.WU32(((uint)(v39 + (uint)(((46)) * 4))), v43);
        M.WU32(0x17C6F0CU, v43);
        F_4BE510(((uint)(M.U16(((uint)(v39 + (uint)(((85)) * 2)))))));
        v39 = M.U32(0x17C68F8U);
        L65: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v39 + (uint)(((47)) * 4)))) != 0)) goto L67;
        v44 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v44);
        M.WU32(((uint)(v39 + (uint)(((47)) * 4))), v44);
        M.WU32(0x17C6D60U, v44);
        F_4BE550(((uint)(M.U16(((uint)(v39 + (uint)(((84)) * 2)))))));
        v39 = M.U32(0x17C68F8U);
        L67: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v39 + (uint)(((48)) * 4)))) != 0)) goto L69;
        v45 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v45);
        M.WU32(((uint)(v39 + (uint)(((48)) * 4))), v45);
        M.WU32(0x17C6D60U, v45);
        F_4BE550(((uint)(M.U16(((uint)(v39 + (uint)(((85)) * 2)))))));
        v39 = M.U32(0x17C68F8U);
        L69: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v39 + (uint)(((49)) * 4)))) != 0)) goto L71;
        v46 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v46);
        M.WU32(((uint)(v39 + (uint)(((49)) * 4))), v46);
        M.WU32(0x17C6BA4U, v46);
        F_4BE4D0(((uint)(M.U16(((uint)(v39 + (uint)(((84)) * 2)))))));
        v39 = M.U32(0x17C68F8U);
        L71: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v39 + (uint)(((50)) * 4)))) != 0)) goto L73;
        v47 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v47);
        M.WU32(((uint)(v39 + (uint)(((50)) * 4))), v47);
        M.WU32(0x17C6BA4U, v47);
        F_4BE4D0(((uint)(M.U16(((uint)(v39 + (uint)(((85)) * 2)))))));
        L73: ;
        F_4CEE70();
        return F_4CEE70();
        return {};
    }

    // sub_4BFB50  int()
    int F_4BFB50()
    {
        int v0 = 0;
        uint v1 = 0;
        uint inserted = 0;
        int v3 = 0;
        F_4CEE10((0));
        v0 = ((int)(M.U32(((uint)(M.I32(0x17C69C8U))))));
        if (!(M.U32(((uint)(M.I32(0x17C69C8U)))) != 0)) goto L1;
        if (!((((uint)(v0)) == ((uint)((-1)))) || (((uint)(v0)) == ((uint)((-2)))))) goto L3;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(M.I32(0x17C69C8U))), v1);
        M.WU32(0x17C68F8U, v1);
        if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
        inserted = F_4CEEC0();
        goto L6;
        L5: ;
        inserted = ((uint)((0)));
        L6: ;
        F_4BF410();
        F_4CAF00();
        if (!(inserted != 0)) goto L7;
        M.WU32(inserted, M.U32(((uint)(M.I32(0x17C69C8U)))));
        L7: ;
        goto L4;
        L3: ;
        M.WU32(((uint)(M.I32(0x17C69C8U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
        L4: ;
        L1: ;
        return F_4CEE70();
        return {};
    }

    // sub_4BFE10  void __cdecl(int a1)
    void F_4BFE10(int a1)
    {
        int v1 = 0;
        byte v2 = 0;
        int v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        uint v6 = 0;
        uint v7 = 0;
        int v8 = 0;
        v1 = a1;
        v2 = ((((uint)(((int)(((uint)((68))) * ((uint)(a1)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v3 = ((int)(((uint)((68))) * ((uint)(a1))));
        v8 = ((int)(((uint)((68))) * ((uint)(a1))));
        if (!!((v2 != 0))) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6A84U), ((uint)((0))), ((uint)(v3)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A84U), ((uint)(v3)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v3) * 1))));
        L1: ;
        v4 = M.U32(0x17C6A84U);
        if (!(v1 > (0))) goto L5;
        v5 = ((uint)(M.U32(0x17C6A84U) + (uint)(((64)) * 1)));
        L7: ;
        v6 = M.U32(v5);
        M.WU32(0x17C6A84U, v4);
        M.WU32(0x17C6EECU, v5);
        if (!(v6 != 0)) goto L10;
        if (!(v6 == ((uint)((-1))))) goto L12;
        v7 = M.U32(0x1EABC3CU);
        M.WU32(v5, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v7)));
        Nat.Load_XStringData(v5);
        goto L13;
        L12: ;
        M.WU32(v5, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v6 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v6 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L13: ;
        L10: ;
        v4 = ((uint)(v4 + (uint)(((68)) * 1)));
        v5 = ((uint)(v5 + (uint)(((17)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        v3 = v8;
        L5: ;
        M.WU32(0x17C6A84U, ((uint)(v4 + (uint)((((int)(0 - v3))) * 1))));
    }

    // sub_4BFEC0  int()
    int F_4BFEC0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6C48U);
        M.WU32(M.U32(0x17C6C48U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6C48U), ((uint)((16))));
        v0 = M.U32(0x17C6C48U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((16)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6C48U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((3)) * 4)))) != 0)) goto L7;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), v3);
        M.WU32(0x17C6A84U, v3);
        F_4BFE10(((int)(M.U32(((uint)(v0 + (uint)(((2)) * 4)))))));
        L7: ;
        return F_4CEE70();
        return {};
    }

    // sub_4BFF80  int()
    int F_4BFF80()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x174686CU))))));
            if (!(M.U32(((uint)(M.I32(0x174686CU)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x174686CU))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x174686CU))), v2);
            M.WU32(0x17C6C48U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4BFEC0();
            v4 = ((uint)(M.I32(0x174686CU)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x174686CU)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((16))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x174686CU)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4C00C0  void __usercall(int result@<eax>)
    void F_4C00C0(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        byte v5 = 0;
        uint v6 = 0;
        uint v7 = 0;
        uint v8 = 0;
        int v9 = 0;
        v1 = result;
        v2 = ((int)(((uint)((12))) * ((uint)(result))));
        v9 = ((int)(((uint)((12))) * ((uint)(result))));
        if (!(((int)(((uint)((12))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6B68U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6B68U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6B68U);
        if (!(v1 > (0))) goto L5;
        v4 = ((uint)(M.U32(0x17C6B68U) + (uint)(((8)) * 1)));
        L7: ;
        v5 = ((M.U32(v3) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6B68U, v3);
        M.WI32(0x17C6EDCU, ((int)(((uint)(v4 - (uint)(((1)) * 4))))));
        if (!!((v5 != 0))) goto L10;
        M.WI32(0x17C6920U, ((int)(((uint)(v4 - (uint)(((1)) * 4))))));
        v6 = M.U32(((uint)(v4 - (uint)(((1)) * 4))));
        M.WI32(0x17C6B60U, ((int)(v4)));
        if (!(v6 >= ((uint)((2))))) goto L12;
        if (!(v6 == ((uint)((2))))) goto L14;
        v7 = M.U32(v4);
        M.WI32(0x17C697CU, ((int)(v4)));
        M.WU32(0x17C6EECU, v4);
        if (!(v7 != 0)) goto L16;
        if (!(v7 == ((uint)((-1))))) goto L18;
        v8 = M.U32(0x1EABC3CU);
        M.WU32(v4, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v8)));
        Nat.Load_XStringData(v4);
        goto L19;
        L18: ;
        M.WU32(v4, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v7 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v7 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L19: ;
        L16: ;
        goto L15;
        L14: ;
        if (!(v6 == ((uint)((3))))) goto L20;
        M.WU32(0x17C6C04U, v4);
        F_4C01C0(((sbyte)((0))));
        L20: ;
        L15: ;
        L12: ;
        L10: ;
        v3 = ((uint)(v3 + (uint)(((3)) * 4)));
        v4 = ((uint)(v4 + (uint)(((3)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        v2 = v9;
        L5: ;
        M.WU32(0x17C6B68U, ((uint)(v3 - (uint)((v2) * 1))));
    }

    // sub_4C01C0  void __cdecl(char a1)
    void F_4C01C0(sbyte a1)
    {
        uint v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        if (!(a1 != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        v1 = M.U32(0x17C6C04U);
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((4)) * 1))));
        M.WU32(M.U32(0x17C6C04U), ((uint)((0))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6C04U), ((uint)((4))));
        v1 = M.U32(0x17C6C04U);
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((4)) * 1))));
        L4: ;
        goto L2;
        L1: ;
        v1 = M.U32(0x17C6C04U);
        L2: ;
        v2 = M.I32(v1);
        if (!(M.I32(v1) != 0)) goto L5;
        if (!(((uint)(v2)) == ((uint)((-1))))) goto L7;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WI32(v1, ((int)(v3)));
        M.WU32(0x17C6AC8U, v3);
        F_4C0260();
        goto L8;
        L7: ;
        M.WI32(v1, ((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v2)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v2)) - ((uint)((1))))))) & ((uint)((268435455))))))))));
        L8: ;
        L5: ;
    }

    // sub_4C0260  void()
    void F_4C0260()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        v0 = M.U32(0x17C6AC8U);
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        M.WU32(M.U32(0x17C6AC8U), ((uint)((0))));
        M.WI32(((uint)(v0 + (uint)(((1)) * 4))), (0));
        M.WI32(((uint)(v0 + (uint)(((2)) * 4))), (0));
        M.WI32(((uint)(v0 + (uint)(((3)) * 4))), (0));
        M.WI32(((uint)(v0 + (uint)(((4)) * 4))), (0));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6AC8U), ((uint)((20))));
        v0 = M.U32(0x17C6AC8U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((20)) * 1))));
        if (!(M.I32(((uint)(v0 + (uint)(((1)) * 4)))) != 0)) goto L3;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WI32(((uint)(v0 + (uint)(((1)) * 4))), ((int)(v1)));
        v2 = M.I32(v0);
        M.WU32(0x1EABC3CU, v1);
        M.WU32(0x17C6B68U, v1);
        F_4C00C0(v2);
        L3: ;
    }

    // sub_4C02D0  void __cdecl(char a1)
    void F_4C02D0(sbyte a1)
    {
        uint v1 = 0;
        if (!(a1 != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        M.WU32(M.U32(0x17C6EA8U), ((uint)((0))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6EA8U), ((uint)((4))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((4)) * 1))));
        L1: ;
        if (!(M.U32(M.U32(0x17C6EA8U)) != 0)) goto L5;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(M.U32(0x17C6EA8U), v1);
        M.WU32(0x17C6E8CU, v1);
        F_4C0630();
        L5: ;
    }

    // sub_4C0330  void()
    void F_4C0330()
    {
        uint v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6CC4U);
        M.WU32(M.U32(0x17C6CC4U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6CC4U), ((uint)((8))));
        v0 = M.U32(0x17C6CC4U);
        L2: ;
        v1 = ((int)(M.U32(((uint)(v0 + (uint)(((1)) * 4))))));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((8)) * 1))));
        v2 = ((uint)(v0 + (uint)(((1)) * 4)));
        M.WU32(0x17C6C04U, ((uint)(v0 + (uint)(((1)) * 4))));
        if (!(v1 != 0)) goto L3;
        if (!(((uint)(v1)) == ((uint)((-1))))) goto L5;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(v2, v3);
        M.WU32(0x17C6AC8U, v3);
        F_4C0260();
        v0 = M.U32(0x17C6CC4U);
        goto L6;
        L5: ;
        M.WU32(v2, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L6: ;
        L3: ;
        M.WU32(0x17C6EA8U, v0);
        F_4C02D0(((sbyte)((0))));
    }

    // sub_4C03E0  void()
    void F_4C03E0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        v0 = M.U32(0x17C6DE8U);
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        M.WU32(M.U32(0x17C6DE8U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6DE8U), ((uint)((8))));
        v0 = M.U32(0x17C6DE8U);
        L2: ;
        v1 = M.U32(v0);
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((8)) * 1))));
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6DE8U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        v3 = ((uint)(v0 + (uint)(((1)) * 4)));
        v4 = M.U32(((uint)(v0 + (uint)(((1)) * 4))));
        M.WU32(0x17C6C04U, v3);
        if (!(v4 != 0)) goto L7;
        if (!(v4 == ((uint)((-1))))) goto L9;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v5);
        M.WU32(v3, v5);
        M.WU32(0x17C6AC8U, v5);
        F_4C0260();
        goto L10;
        L9: ;
        M.WU32(v3, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v4 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v4 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L10: ;
        L7: ;
    }

    // sub_4C04B0  void()
    void F_4C04B0()
    {
        sbyte v0 = 0;
        byte v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint _t1 = 0;
        v0 = ((sbyte)(M.U8(((uint)(((int)(((uint)(M.I32(0x17C6B4CU))) + ((uint)((4))))))))));
        if (!(v0 != 0)) goto L1;
        _t1 = ((uint)(v0));
        if (_t1 == 0x1U) goto L4;
        if (_t1 == 0x2U) goto L5;
        if (_t1 == 0x3U || _t1 == 0x4U || _t1 == 0x5U || _t1 == 0x6U) goto L6;
        goto L3;
        L4: ;
        v1 = ((M.U32(((uint)(M.I32(0x17C6EC4U)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WI32(0x17C6C08U, M.I32(0x17C6EC4U));
        if (!!((v1 != 0))) goto L7;
        v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v2);
        M.WU32(((uint)(M.I32(0x17C6EC4U))), v2);
        M.WU32(0x17C6CC4U, v2);
        F_4C0330();
        L7: ;
        goto L3;
        L5: ;
        M.WU32(0x17C6EA8U, ((uint)(M.I32(0x17C6EC4U))));
        F_4C02D0(((sbyte)((0))));
        goto L3;
        L6: ;
        v1 = ((M.U32(((uint)(M.I32(0x17C6EC4U)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WI32(0x17C6984U, M.I32(0x17C6EC4U));
        if (!!((v1 != 0))) goto L9;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(M.I32(0x17C6EC4U))), v3);
        M.WU32(0x17C6DE8U, v3);
        F_4C03E0();
        L9: ;
        goto L3;
        L3: ;
        goto L2;
        L1: ;
        M.WU32(0x17C6EECU, ((uint)(M.I32(0x17C6EC4U))));
        F_4B7070(((sbyte)((0))));
        L2: ;
    }

    // sub_4C0570  void __usercall(int a1@<eax>)
    void F_4C0570(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        byte v4 = 0;
        uint v5 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((4))) * ((uint)(a1))));
        if (!(((int)(((uint)((4))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6D08U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6D08U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6D08U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = ((M.U32(v3) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6D08U, v3);
        if (!!((v4 != 0))) goto L10;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v4 = ((((uint)(M.I32(0x1EBDC44U))) == ((uint)((2)))) ? (byte)1 : (byte)0);
        M.WU32(0x1EABC3CU, v5);
        M.WU32(v3, v5);
        M.WI32(0x17C6B4CU, ((int)(v5)));
        if (!(v4 != 0)) goto L12;
        M.WU32(v5, ((uint)((0))));
        M.WU32(((uint)(v5 + (uint)(((1)) * 4))), ((uint)((0))));
        goto L13;
        L12: ;
        Nat.DB_LoadXFileData(v5, ((uint)((8))));
        v5 = M.U32(0x1EABC3CU);
        L13: ;
        M.WU32(0x1EABC3CU, ((uint)(v5 + (uint)(((2)) * 4))));
        M.WI32(0x17C6EC4U, M.I32(0x17C6B4CU));
        F_4C04B0();
        L10: ;
        Rt::PreAdd(v3, 4);
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6D08U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4C0630  void()
    void F_4C0630()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        v0 = M.U32(0x17C6E8CU);
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        M.WU32(M.U32(0x17C6E8CU), ((uint)((0))));
        M.WI32(((uint)(v0 + (uint)(((1)) * 4))), (0));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6E8CU), ((uint)((8))));
        v0 = M.U32(0x17C6E8CU);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((8)) * 1))));
        if (!(M.I32(((uint)(v0 + (uint)(((1)) * 4)))) != 0)) goto L3;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WI32(((uint)(v0 + (uint)(((1)) * 4))), ((int)(v1)));
        v2 = M.I32(v0);
        M.WU32(0x1EABC3CU, v1);
        M.WU32(0x17C6D08U, v1);
        F_4C0570(v2);
        L3: ;
    }

    // sub_4C0690  int()
    int F_4C0690()
    {
        byte v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        Nat.Memset(M.U32(0x17C691CU), ((uint)((0))), ((uint)((324))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C691CU), ((uint)((324))));
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((324)) * 1))));
        v0 = ((M.U32(((uint)(M.U32(0x17C691CU) + (uint)(((72)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6EA8U, ((uint)(M.U32(0x17C691CU) + (uint)(((288)) * 1))));
        if (!!((v0 != 0))) goto L3;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(M.U32(0x17C691CU) + (uint)(((72)) * 4))), v1);
        M.WU32(0x17C6E8CU, v1);
        F_4C0630();
        L3: ;
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C691CU) + (uint)(((320)) * 1))));
        return F_4BA420();
        return {};
    }

    // sub_4C0720  void()
    void F_4C0720()
    {
        uint v0 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6A0CU);
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((32)) * 1))));
        M.WU32(M.U32(0x17C6A0CU), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((4)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((5)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((6)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((7)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A0CU), ((uint)((32))));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((32)) * 1))));
        L2: ;
    }

    // sub_4C0770  int()
    int F_4C0770()
    {
        int v0 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        Nat.Memset(M.U32(0x17C6E24U), ((uint)((0))), ((uint)((392))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6E24U), ((uint)((392))));
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((392)) * 1))));
        M.WU32(0x17C6EECU, M.U32(0x17C6E24U));
        F_4B70F0((32));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6E24U) + (uint)(((128)) * 1))));
        return F_4B70F0((32));
        return {};
    }

    // sub_4C07E0  int()
    int F_4C07E0()
    {
        uint v0 = 0;
        int v1 = 0;
        v0 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v0);
        M.WU32(((uint)(M.I32(0x17C6B40U))), v0);
        M.WU32(0x17C6E24U, v0);
        return F_4C0770();
        return {};
    }

    // sub_4C0810  void()
    void F_4C0810()
    {
        uint v0 = 0;
        byte v1 = 0;
        v0 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v1 = ((((uint)(M.I32(0x1EBDC44U))) == ((uint)((2)))) ? (byte)1 : (byte)0);
        M.WU32(0x1EABC3CU, v0);
        M.WU32(((uint)(M.I32(0x17C6A9CU))), v0);
        M.WI32(0x17C6A80U, ((int)(v0)));
        if (!(v1 != 0)) goto L1;
        M.WU32(v0, ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((4)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((5)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((6)) * 4))), ((uint)((0))));
        M.WU32(0x1EABC3CU, ((uint)(v0 + (uint)(((7)) * 4))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(v0, ((uint)((28))));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((28)) * 1))));
        L2: ;
    }

    // sub_4C0870  void()
    void F_4C0870()
    {
        uint v0 = 0;
        byte v1 = 0;
        v0 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v1 = ((((uint)(M.I32(0x1EBDC44U))) == ((uint)((2)))) ? (byte)1 : (byte)0);
        M.WU32(0x1EABC3CU, v0);
        M.WU32(((uint)(M.I32(0x17C6C5CU))), v0);
        M.WI32(0x17C6B20U, ((int)(v0)));
        if (!(v1 != 0)) goto L1;
        M.WU32(v0, ((uint)((0))));
        M.WU32(0x1EABC3CU, ((uint)(v0 + (uint)(((1)) * 4))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(v0, ((uint)((4))));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((4)) * 1))));
        L2: ;
    }

    // sub_4C08C0  int()
    int F_4C08C0()
    {
        uint v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        int v6 = 0;
        v0 = M.U32(0x17C6D0CU);
        v1 = ((int)(M.U32(M.U32(0x17C6D0CU))));
        M.WU32(0x17C6EECU, M.U32(0x17C6D0CU));
        if (!(v1 != 0)) goto L1;
        if (!(((uint)(v1)) == ((uint)((-1))))) goto L3;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(M.U32(0x17C6D0CU), M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6D0CU);
        goto L4;
        L3: ;
        M.WU32(M.U32(0x17C6D0CU), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L4: ;
        L1: ;
        v3 = M.U32(((uint)(v0 + (uint)(((11)) * 4))));
        v4 = ((uint)(v0 + (uint)(((11)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v0 + (uint)(((11)) * 4))));
        if (!(v3 != 0)) goto L5;
        if (!(v3 == ((uint)((-1))))) goto L7;
        v5 = M.U32(0x1EABC3CU);
        M.WU32(v4, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v5)));
        Nat.Load_XStringData(v4);
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6D0CU) + (uint)(((40)) * 4))));
        return F_4BA420();
        L7: ;
        M.WU32(v4, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v3 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v3 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L5: ;
        M.WU32(0x17C698CU, ((uint)(v0 + (uint)(((40)) * 4))));
        return F_4BA420();
        return {};
    }

    // sub_4C0980  void __cdecl(char a1)
    void F_4C0980(sbyte a1)
    {
        uint v1 = 0;
        byte v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        if (!(a1 != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        v1 = M.U32(0x17C6ADCU);
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((12)) * 1))));
        M.WU32(M.U32(0x17C6ADCU), ((uint)((0))));
        M.WU32(((uint)(v1 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v1 + (uint)(((2)) * 4))), ((uint)((0))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6ADCU), ((uint)((12))));
        v1 = M.U32(0x17C6ADCU);
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((12)) * 1))));
        L4: ;
        goto L2;
        L1: ;
        v1 = M.U32(0x17C6ADCU);
        L2: ;
        v2 = ((M.U32(((uint)(v1 + (uint)(((1)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6EA8U, ((uint)(v1 + (uint)(((1)) * 4))));
        if (!!((v2 != 0))) goto L5;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(v1 + (uint)(((1)) * 4))), v3);
        M.WU32(0x17C6E8CU, v3);
        F_4C0630();
        v1 = M.U32(0x17C6ADCU);
        L5: ;
        if (!(M.U32(((uint)(v1 + (uint)(((2)) * 4)))) != 0)) goto L7;
        v4 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v4);
        M.WU32(((uint)(v1 + (uint)(((2)) * 4))), v4);
        M.WU32(0x17C6D74U, v4);
        F_4C0A30();
        L7: ;
    }

    // sub_4C0A30  void()
    void F_4C0A30()
    {
        uint v0 = 0;
        v0 = M.U32(0x17C6D74U);
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        M.WU32(M.U32(0x17C6D74U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6D74U), ((uint)((12))));
        v0 = M.U32(0x17C6D74U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((12)) * 1))));
        M.WU32(0x17C6ADCU, v0);
        F_4C0980(((sbyte)((0))));
    }

    // sub_4C0A80  void()
    void F_4C0A80()
    {
        int v0 = 0;
        byte v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint _t1 = 0;
        v0 = ((int)(M.U32(((uint)(M.U32(0x17C6C10U) + (uint)(((46)) * 4))))));
        _t1 = ((uint)(v0));
        if (_t1 == 0x6U) goto L2;
        if (_t1 == 0x4U || _t1 == 0x9U || _t1 == 0x10U || _t1 == 0x12U || _t1 == 0xBU || _t1 == 0xEU || _t1 == 0xAU || _t1 == 0x0U || _t1 == 0x11U || _t1 == 0x16U || _t1 == 0x17U) goto L3;
        if (_t1 == 0xCU) goto L4;
        if (_t1 == 0xDU) goto L5;
        if (_t1 == 0x14U) goto L6;
        if (_t1 == 0x15U) goto L7;
        goto L1;
        L2: ;
        v1 = ((M.U32(((uint)(M.I32(0x17C6CECU)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WI32(0x17C6F60U, M.I32(0x17C6CECU));
        if (!!((v1 != 0))) goto L8;
        v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v2);
        M.WU32(((uint)(M.I32(0x17C6CECU))), v2);
        M.WU32(0x17C691CU, v2);
        F_4C0690();
        L8: ;
        goto L1;
        L3: ;
        v1 = ((M.U32(((uint)(M.I32(0x17C6CECU)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WI32(0x17C6D38U, M.I32(0x17C6CECU));
        if (!!((v1 != 0))) goto L10;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(M.I32(0x17C6CECU))), v3);
        M.WU32(0x17C6A0CU, v3);
        F_4C0720();
        L10: ;
        goto L1;
        L4: ;
        v1 = ((M.U32(((uint)(M.I32(0x17C6CECU)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WI32(0x17C6B40U, M.I32(0x17C6CECU));
        if (!!((v1 != 0))) goto L12;
        F_4C07E0();
        L12: ;
        goto L1;
        L5: ;
        M.WU32(0x17C6EECU, ((uint)(M.I32(0x17C6CECU))));
        F_4B7070(((sbyte)((0))));
        goto L1;
        L6: ;
        v1 = ((M.U32(((uint)(M.I32(0x17C6CECU)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WI32(0x17C6A9CU, M.I32(0x17C6CECU));
        if (!!((v1 != 0))) goto L14;
        F_4C0810();
        L14: ;
        goto L1;
        L7: ;
        v1 = ((M.U32(((uint)(M.I32(0x17C6CECU)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WI32(0x17C6C5CU, M.I32(0x17C6CECU));
        if (!!((v1 != 0))) goto L16;
        F_4C0870();
        L16: ;
        goto L1;
        L1: ;
    }

    // sub_4C0BC0  void __usercall(int result@<eax>)
    void F_4C0BC0(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        uint v5 = 0;
        uint v6 = 0;
        int v7 = 0;
        v1 = result;
        v2 = ((int)(((uint)((8))) * ((uint)(result))));
        v7 = ((int)(((uint)((8))) * ((uint)(result))));
        if (!(((int)(((uint)((8))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6AACU), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6AACU), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6AACU);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = ((int)(M.U32(((uint)(v3 + (uint)(((1)) * 4))))));
        v5 = ((uint)(v3 + (uint)(((1)) * 4)));
        M.WU32(0x17C6AACU, v3);
        M.WU32(0x17C6C04U, ((uint)(v3 + (uint)(((1)) * 4))));
        if (!(v4 != 0)) goto L10;
        if (!(((uint)(v4)) == ((uint)((-1))))) goto L12;
        v6 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v6);
        M.WU32(v5, v6);
        M.WU32(0x17C6AC8U, v6);
        F_4C0260();
        goto L13;
        L12: ;
        v2 = v7;
        M.WU32(v5, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v4)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v4)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L13: ;
        L10: ;
        v3 = ((uint)(v3 + (uint)(((2)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6AACU, ((uint)(v3 - (uint)((v2) * 1))));
    }

    // sub_4C0C90  void()
    void F_4C0C90()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        byte v5 = 0;
        uint v6 = 0;
        uint v7 = 0;
        uint v8 = 0;
        uint v9 = 0;
        uint v10 = 0;
        uint v11 = 0;
        uint v12 = 0;
        uint v13 = 0;
        uint v14 = 0;
        uint v15 = 0;
        uint v16 = 0;
        uint v17 = 0;
        uint v18 = 0;
        uint v19 = 0;
        uint v20 = 0;
        uint v21 = 0;
        uint v22 = 0;
        uint v23 = 0;
        uint v24 = 0;
        uint v25 = 0;
        uint v26 = 0;
        uint v27 = 0;
        uint v28 = 0;
        int v29 = 0;
        uint v30 = 0;
        uint v31 = 0;
        int v32 = 0;
        uint v33 = 0;
        uint v34 = 0;
        int v35 = 0;
        uint v36 = 0;
        uint v37 = 0;
        int v38 = 0;
        uint v39 = 0;
        uint v40 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6C10U);
        Nat.Memset(M.U32(0x17C6C10U), ((uint)((0))), ((uint)((380))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6C10U), ((uint)((380))));
        v0 = M.U32(0x17C6C10U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((380)) * 1))));
        M.WI32(0x17C6F40U, ((int)(v0)));
        M.WU32(0x17C6D0CU, v0);
        F_4C08C0();
        v1 = M.U32(0x17C6C10U);
        v2 = ((int)(M.U32(((uint)(M.U32(0x17C6C10U) + (uint)(((57)) * 4))))));
        v3 = ((uint)(M.U32(0x17C6C10U) + (uint)(((228)) * 1)));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6C10U) + (uint)(((228)) * 1))));
        if (!(v2 != 0)) goto L3;
        if (!(((uint)(v2)) == ((uint)((-1))))) goto L5;
        v4 = M.U32(0x1EABC3CU);
        M.WU32(v3, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v4)));
        Nat.Load_XStringData(v3);
        v1 = M.U32(0x17C6C10U);
        goto L6;
        L5: ;
        M.WU32(v3, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v2)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v2)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L6: ;
        L3: ;
        v5 = ((M.U32(((uint)(v1 + (uint)(((60)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6EA8U, ((uint)(v1 + (uint)(((60)) * 4))));
        if (!!((v5 != 0))) goto L7;
        v6 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v6);
        M.WU32(((uint)(v1 + (uint)(((60)) * 4))), v6);
        M.WU32(0x17C6E8CU, v6);
        F_4C0630();
        v1 = M.U32(0x17C6C10U);
        L7: ;
        v5 = ((M.U32(((uint)(v1 + (uint)(((61)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6EA8U, ((uint)(v1 + (uint)(((61)) * 4))));
        if (!!((v5 != 0))) goto L9;
        v7 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v7);
        M.WU32(((uint)(v1 + (uint)(((61)) * 4))), v7);
        M.WU32(0x17C6E8CU, v7);
        F_4C0630();
        v1 = M.U32(0x17C6C10U);
        L9: ;
        v5 = ((M.U32(((uint)(v1 + (uint)(((62)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6EA8U, ((uint)(v1 + (uint)(((62)) * 4))));
        if (!!((v5 != 0))) goto L11;
        v8 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v8);
        M.WU32(((uint)(v1 + (uint)(((62)) * 4))), v8);
        M.WU32(0x17C6E8CU, v8);
        F_4C0630();
        v1 = M.U32(0x17C6C10U);
        L11: ;
        v5 = ((M.U32(((uint)(v1 + (uint)(((63)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6EA8U, ((uint)(v1 + (uint)(((63)) * 4))));
        if (!!((v5 != 0))) goto L13;
        v9 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v9);
        M.WU32(((uint)(v1 + (uint)(((63)) * 4))), v9);
        M.WU32(0x17C6E8CU, v9);
        F_4C0630();
        v1 = M.U32(0x17C6C10U);
        L13: ;
        v5 = ((M.U32(((uint)(v1 + (uint)(((64)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6EA8U, ((uint)(v1 + (uint)(((64)) * 4))));
        if (!!((v5 != 0))) goto L15;
        v10 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v10);
        M.WU32(((uint)(v1 + (uint)(((64)) * 4))), v10);
        M.WU32(0x17C6E8CU, v10);
        F_4C0630();
        v1 = M.U32(0x17C6C10U);
        L15: ;
        v5 = ((M.U32(((uint)(v1 + (uint)(((65)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6EA8U, ((uint)(v1 + (uint)(((65)) * 4))));
        if (!!((v5 != 0))) goto L17;
        v11 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v11);
        M.WU32(((uint)(v1 + (uint)(((65)) * 4))), v11);
        M.WU32(0x17C6E8CU, v11);
        F_4C0630();
        v1 = M.U32(0x17C6C10U);
        L17: ;
        v5 = ((M.U32(((uint)(v1 + (uint)(((66)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6EA8U, ((uint)(v1 + (uint)(((66)) * 4))));
        if (!!((v5 != 0))) goto L19;
        v12 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v12);
        M.WU32(((uint)(v1 + (uint)(((66)) * 4))), v12);
        M.WU32(0x17C6E8CU, v12);
        F_4C0630();
        v1 = M.U32(0x17C6C10U);
        L19: ;
        v5 = ((M.U32(((uint)(v1 + (uint)(((67)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6EA8U, ((uint)(v1 + (uint)(((67)) * 4))));
        if (!!((v5 != 0))) goto L21;
        v13 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v13);
        M.WU32(((uint)(v1 + (uint)(((67)) * 4))), v13);
        M.WU32(0x17C6E8CU, v13);
        F_4C0630();
        v1 = M.U32(0x17C6C10U);
        L21: ;
        v14 = M.U32(((uint)(v1 + (uint)(((68)) * 4))));
        v15 = ((uint)(v1 + (uint)(((68)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v1 + (uint)(((68)) * 4))));
        if (!(v14 != 0)) goto L23;
        if (!(v14 == ((uint)((-1))))) goto L25;
        v16 = M.U32(0x1EABC3CU);
        M.WU32(v15, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v16)));
        Nat.Load_XStringData(v15);
        v1 = M.U32(0x17C6C10U);
        goto L26;
        L25: ;
        M.WU32(v15, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v14 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v14 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L26: ;
        L23: ;
        v17 = M.U32(((uint)(v1 + (uint)(((69)) * 4))));
        v18 = ((uint)(v1 + (uint)(((69)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v1 + (uint)(((69)) * 4))));
        if (!(v17 != 0)) goto L27;
        if (!(v17 == ((uint)((-1))))) goto L29;
        v19 = M.U32(0x1EABC3CU);
        M.WU32(v18, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v19)));
        Nat.Load_XStringData(v18);
        v1 = M.U32(0x17C6C10U);
        goto L30;
        L29: ;
        M.WU32(v18, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v17 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v17 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L30: ;
        L27: ;
        if (!(M.U32(((uint)(v1 + (uint)(((70)) * 4)))) != 0)) goto L31;
        v20 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v20);
        M.WU32(((uint)(v1 + (uint)(((70)) * 4))), v20);
        M.WU32(0x17C6ADCU, v20);
        F_4C0980(((sbyte)((1))));
        v1 = M.U32(0x17C6C10U);
        L31: ;
        v21 = M.U32(((uint)(v1 + (uint)(((71)) * 4))));
        v22 = ((uint)(v1 + (uint)(((71)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v1 + (uint)(((71)) * 4))));
        if (!(v21 != 0)) goto L33;
        if (!(v21 == ((uint)((-1))))) goto L35;
        v23 = M.U32(0x1EABC3CU);
        M.WU32(v22, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v23)));
        Nat.Load_XStringData(v22);
        v1 = M.U32(0x17C6C10U);
        goto L36;
        L35: ;
        M.WU32(v22, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v21 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v21 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L36: ;
        L33: ;
        v24 = M.U32(((uint)(v1 + (uint)(((72)) * 4))));
        v25 = ((uint)(v1 + (uint)(((72)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v1 + (uint)(((72)) * 4))));
        if (!(v24 != 0)) goto L37;
        if (!(v24 == ((uint)((-1))))) goto L39;
        v26 = M.U32(0x1EABC3CU);
        M.WU32(v25, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v26)));
        Nat.Load_XStringData(v25);
        v1 = M.U32(0x17C6C10U);
        goto L40;
        L39: ;
        M.WU32(v25, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v24 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v24 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L40: ;
        L37: ;
        M.WI32(0x17C6A64U, ((int)(((uint)(v1 + (uint)(((74)) * 4))))));
        F_4B8830();
        M.WI32(0x17C6CECU, ((int)(((uint)(((int)(M.U32(0x17C6C10U))))) + ((uint)((308))))));
        F_4C0A80();
        v27 = M.U32(0x17C6C10U);
        if (!(M.U32(((uint)(M.U32(0x17C6C10U) + (uint)(((80)) * 4)))) != 0)) goto L41;
        v28 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v28);
        M.WU32(((uint)(M.U32(0x17C6C10U) + (uint)(((80)) * 4))), v28);
        M.WU32(0x17C6AACU, v28);
        F_4C0BC0(M.I32(((uint)(v27 + (uint)(((79)) * 4)))));
        v27 = M.U32(0x17C6C10U);
        L41: ;
        v29 = M.I32(((uint)(v27 + (uint)(((81)) * 4))));
        v30 = ((uint)(v27 + (uint)(((81)) * 4)));
        M.WU32(0x17C6C04U, ((uint)(v27 + (uint)(((81)) * 4))));
        if (!(v29 != 0)) goto L43;
        if (!(((uint)(v29)) == ((uint)((-1))))) goto L45;
        v31 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v31);
        M.WU32(v30, v31);
        M.WU32(0x17C6AC8U, v31);
        F_4C0260();
        v27 = M.U32(0x17C6C10U);
        goto L46;
        L45: ;
        M.WU32(v30, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v29)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v29)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L46: ;
        L43: ;
        v32 = M.I32(((uint)(v27 + (uint)(((82)) * 4))));
        v33 = ((uint)(v27 + (uint)(((82)) * 4)));
        M.WU32(0x17C6C04U, ((uint)(v27 + (uint)(((82)) * 4))));
        if (!(v32 != 0)) goto L47;
        if (!(((uint)(v32)) == ((uint)((-1))))) goto L49;
        v34 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v34);
        M.WU32(v33, v34);
        M.WU32(0x17C6AC8U, v34);
        F_4C0260();
        v27 = M.U32(0x17C6C10U);
        goto L50;
        L49: ;
        M.WU32(v33, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v32)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v32)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L50: ;
        L47: ;
        v35 = M.I32(((uint)(v27 + (uint)(((83)) * 4))));
        v36 = ((uint)(v27 + (uint)(((83)) * 4)));
        M.WU32(0x17C6C04U, ((uint)(v27 + (uint)(((83)) * 4))));
        if (!(v35 != 0)) goto L51;
        if (!(((uint)(v35)) == ((uint)((-1))))) goto L53;
        v37 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v37);
        M.WU32(v36, v37);
        M.WU32(0x17C6AC8U, v37);
        F_4C0260();
        v27 = M.U32(0x17C6C10U);
        goto L54;
        L53: ;
        M.WU32(v36, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v35)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v35)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L54: ;
        L51: ;
        v38 = M.I32(((uint)(v27 + (uint)(((84)) * 4))));
        v39 = ((uint)(v27 + (uint)(((84)) * 4)));
        M.WU32(0x17C6C04U, ((uint)(v27 + (uint)(((84)) * 4))));
        if (!(v38 != 0)) goto L55;
        if (!(((uint)(v38)) == ((uint)((-1))))) goto L57;
        v40 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v40);
        M.WU32(v39, v40);
        M.WU32(0x17C6AC8U, v40);
        F_4C0260();
        goto L58;
        L57: ;
        M.WU32(v39, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v38)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v38)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L58: ;
        L55: ;
    }

    // sub_4C1230  void __usercall(int result@<eax>)
    void F_4C1230(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        byte v4 = 0;
        uint v5 = 0;
        v1 = result;
        v2 = ((int)(((uint)((4))) * ((uint)(result))));
        if (!(((int)(((uint)((4))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C68ECU), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C68ECU), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C68ECU);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = ((M.U32(v3) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C68ECU, v3);
        if (!!((v4 != 0))) goto L10;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v5);
        M.WU32(v3, v5);
        M.WU32(0x17C6C10U, v5);
        F_4C0C90();
        L10: ;
        Rt::PreAdd(v3, 4);
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C68ECU, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4C12B0  int()
    int F_4C12B0()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        byte v5 = 0;
        uint v6 = 0;
        uint v7 = 0;
        uint v8 = 0;
        uint v9 = 0;
        uint v10 = 0;
        int v11 = 0;
        uint v12 = 0;
        uint v13 = 0;
        int v14 = 0;
        uint v15 = 0;
        uint v16 = 0;
        int v17 = 0;
        uint v18 = 0;
        uint v19 = 0;
        int v20 = 0;
        uint v21 = 0;
        uint v22 = 0;
        int v23 = 0;
        uint v24 = 0;
        uint v25 = 0;
        int v26 = 0;
        uint v27 = 0;
        uint v28 = 0;
        int v29 = 0;
        uint v30 = 0;
        uint v31 = 0;
        int v32 = 0;
        uint v33 = 0;
        uint v34 = 0;
        int v35 = 0;
        uint v36 = 0;
        uint v37 = 0;
        uint v38 = 0;
        int v39 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6DB4U);
        Nat.Memset(M.U32(0x17C6DB4U), ((uint)((0))), ((uint)((400))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6DB4U), ((uint)((400))));
        v0 = M.U32(0x17C6DB4U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((400)) * 1))));
        F_4CEE10((3));
        M.WI32(0x17C6F40U, ((int)(v0)));
        M.WU32(0x17C6D0CU, v0);
        F_4C08C0();
        v1 = M.U32(0x17C6DB4U);
        v2 = ((int)(M.U32(((uint)(M.U32(0x17C6DB4U) + (uint)(((41)) * 4))))));
        v3 = ((uint)(M.U32(0x17C6DB4U) + (uint)(((164)) * 1)));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6DB4U) + (uint)(((164)) * 1))));
        if (!(v2 != 0)) goto L3;
        if (!(((uint)(v2)) == ((uint)((-1))))) goto L5;
        v4 = M.U32(0x1EABC3CU);
        M.WU32(v3, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v4)));
        Nat.Load_XStringData(v3);
        v1 = M.U32(0x17C6DB4U);
        goto L6;
        L5: ;
        M.WU32(v3, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v2)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v2)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L6: ;
        L3: ;
        v5 = ((M.U32(((uint)(v1 + (uint)(((52)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6EA8U, ((uint)(v1 + (uint)(((52)) * 4))));
        if (!!((v5 != 0))) goto L7;
        v6 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v6);
        M.WU32(((uint)(v1 + (uint)(((52)) * 4))), v6);
        M.WU32(0x17C6E8CU, v6);
        F_4C0630();
        v1 = M.U32(0x17C6DB4U);
        L7: ;
        v5 = ((M.U32(((uint)(v1 + (uint)(((54)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6EA8U, ((uint)(v1 + (uint)(((54)) * 4))));
        if (!!((v5 != 0))) goto L9;
        v7 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v7);
        M.WU32(((uint)(v1 + (uint)(((54)) * 4))), v7);
        M.WU32(0x17C6E8CU, v7);
        F_4C0630();
        v1 = M.U32(0x17C6DB4U);
        L9: ;
        v5 = ((M.U32(((uint)(v1 + (uint)(((53)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6EA8U, ((uint)(v1 + (uint)(((53)) * 4))));
        if (!!((v5 != 0))) goto L11;
        v8 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v8);
        M.WU32(((uint)(v1 + (uint)(((53)) * 4))), v8);
        M.WU32(0x17C6E8CU, v8);
        F_4C0630();
        v1 = M.U32(0x17C6DB4U);
        L11: ;
        v5 = ((M.U32(((uint)(v1 + (uint)(((55)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6EA8U, ((uint)(v1 + (uint)(((55)) * 4))));
        if (!!((v5 != 0))) goto L13;
        v9 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v9);
        M.WU32(((uint)(v1 + (uint)(((55)) * 4))), v9);
        M.WU32(0x17C6E8CU, v9);
        F_4C0630();
        v1 = M.U32(0x17C6DB4U);
        L13: ;
        if (!(M.U32(((uint)(v1 + (uint)(((56)) * 4)))) != 0)) goto L15;
        v10 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v10);
        M.WU32(((uint)(v1 + (uint)(((56)) * 4))), v10);
        M.WU32(0x17C6ADCU, v10);
        F_4C0980(((sbyte)((1))));
        v1 = M.U32(0x17C6DB4U);
        L15: ;
        v11 = ((int)(M.U32(((uint)(v1 + (uint)(((57)) * 4))))));
        v12 = ((uint)(v1 + (uint)(((57)) * 4)));
        M.WU32(0x17C6C04U, ((uint)(v1 + (uint)(((57)) * 4))));
        if (!(v11 != 0)) goto L17;
        if (!(((uint)(v11)) == ((uint)((-1))))) goto L19;
        v13 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v13);
        M.WU32(v12, v13);
        M.WU32(0x17C6AC8U, v13);
        F_4C0260();
        v1 = M.U32(0x17C6DB4U);
        goto L20;
        L19: ;
        M.WU32(v12, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v11)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v11)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L20: ;
        L17: ;
        v14 = ((int)(M.U32(((uint)(v1 + (uint)(((58)) * 4))))));
        v15 = ((uint)(v1 + (uint)(((58)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v1 + (uint)(((58)) * 4))));
        if (!(v14 != 0)) goto L21;
        if (!(((uint)(v14)) == ((uint)((-1))))) goto L23;
        v16 = M.U32(0x1EABC3CU);
        M.WU32(v15, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v16)));
        Nat.Load_XStringData(v15);
        v1 = M.U32(0x17C6DB4U);
        goto L24;
        L23: ;
        M.WU32(v15, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v14)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v14)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L24: ;
        L21: ;
        v17 = ((int)(M.U32(((uint)(v1 + (uint)(((59)) * 4))))));
        v18 = ((uint)(v1 + (uint)(((59)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v1 + (uint)(((59)) * 4))));
        if (!(v17 != 0)) goto L25;
        if (!(((uint)(v17)) == ((uint)((-1))))) goto L27;
        v19 = M.U32(0x1EABC3CU);
        M.WU32(v18, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v19)));
        Nat.Load_XStringData(v18);
        v1 = M.U32(0x17C6DB4U);
        goto L28;
        L27: ;
        M.WU32(v18, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v17)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v17)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L28: ;
        L25: ;
        v20 = ((int)(M.U32(((uint)(v1 + (uint)(((65)) * 4))))));
        v21 = ((uint)(v1 + (uint)(((65)) * 4)));
        M.WU32(0x17C6C04U, ((uint)(v1 + (uint)(((65)) * 4))));
        if (!(v20 != 0)) goto L29;
        if (!(((uint)(v20)) == ((uint)((-1))))) goto L31;
        v22 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v22);
        M.WU32(v21, v22);
        M.WU32(0x17C6AC8U, v22);
        F_4C0260();
        v1 = M.U32(0x17C6DB4U);
        goto L32;
        L31: ;
        M.WU32(v21, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v20)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v20)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L32: ;
        L29: ;
        v23 = ((int)(M.U32(((uint)(v1 + (uint)(((66)) * 4))))));
        v24 = ((uint)(v1 + (uint)(((66)) * 4)));
        M.WU32(0x17C6C04U, ((uint)(v1 + (uint)(((66)) * 4))));
        if (!(v23 != 0)) goto L33;
        if (!(((uint)(v23)) == ((uint)((-1))))) goto L35;
        v25 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v25);
        M.WU32(v24, v25);
        M.WU32(0x17C6AC8U, v25);
        F_4C0260();
        v1 = M.U32(0x17C6DB4U);
        goto L36;
        L35: ;
        M.WU32(v24, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v23)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v23)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L36: ;
        L33: ;
        v26 = ((int)(M.U32(((uint)(v1 + (uint)(((67)) * 4))))));
        v27 = ((uint)(v1 + (uint)(((67)) * 4)));
        M.WU32(0x17C6C04U, ((uint)(v1 + (uint)(((67)) * 4))));
        if (!(v26 != 0)) goto L37;
        if (!(((uint)(v26)) == ((uint)((-1))))) goto L39;
        v28 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v28);
        M.WU32(v27, v28);
        M.WU32(0x17C6AC8U, v28);
        F_4C0260();
        v1 = M.U32(0x17C6DB4U);
        goto L40;
        L39: ;
        M.WU32(v27, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v26)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v26)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L40: ;
        L37: ;
        v29 = ((int)(M.U32(((uint)(v1 + (uint)(((68)) * 4))))));
        v30 = ((uint)(v1 + (uint)(((68)) * 4)));
        M.WU32(0x17C6C04U, ((uint)(v1 + (uint)(((68)) * 4))));
        if (!(v29 != 0)) goto L41;
        if (!(((uint)(v29)) == ((uint)((-1))))) goto L43;
        v31 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v31);
        M.WU32(v30, v31);
        M.WU32(0x17C6AC8U, v31);
        F_4C0260();
        v1 = M.U32(0x17C6DB4U);
        goto L44;
        L43: ;
        M.WU32(v30, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v29)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v29)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L44: ;
        L41: ;
        v32 = ((int)(M.U32(((uint)(v1 + (uint)(((69)) * 4))))));
        v33 = ((uint)(v1 + (uint)(((69)) * 4)));
        M.WU32(0x17C6C04U, ((uint)(v1 + (uint)(((69)) * 4))));
        if (!(v32 != 0)) goto L45;
        if (!(((uint)(v32)) == ((uint)((-1))))) goto L47;
        v34 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v34);
        M.WU32(v33, v34);
        M.WU32(0x17C6AC8U, v34);
        F_4C0260();
        v1 = M.U32(0x17C6DB4U);
        goto L48;
        L47: ;
        M.WU32(v33, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v32)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v32)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L48: ;
        L45: ;
        v35 = ((int)(M.U32(((uint)(v1 + (uint)(((70)) * 4))))));
        v36 = ((uint)(v1 + (uint)(((70)) * 4)));
        M.WU32(0x17C6C04U, ((uint)(v1 + (uint)(((70)) * 4))));
        if (!(v35 != 0)) goto L49;
        if (!(((uint)(v35)) == ((uint)((-1))))) goto L51;
        v37 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v37);
        M.WU32(v36, v37);
        M.WU32(0x17C6AC8U, v37);
        F_4C0260();
        v1 = M.U32(0x17C6DB4U);
        goto L52;
        L51: ;
        M.WU32(v36, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v35)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v35)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L52: ;
        L49: ;
        if (!(M.U32(((uint)(v1 + (uint)(((71)) * 4)))) != 0)) goto L53;
        v38 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v38);
        M.WU32(((uint)(v1 + (uint)(((71)) * 4))), v38);
        M.WU32(0x17C68ECU, v38);
        F_4C1230(((int)(M.U32(((uint)(v1 + (uint)(((44)) * 4)))))));
        L53: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C17B0  int()
    int F_4C17B0()
    {
        int v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        int v3 = 0;
        F_4CEE10((0));
        v0 = ((int)(M.U32(M.U32(0x17C6C64U))));
        if (!(M.U32(M.U32(0x17C6C64U)) != 0)) goto L1;
        if (!((((uint)(v0)) == ((uint)((-1)))) || (((uint)(v0)) == ((uint)((-2)))))) goto L3;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(M.U32(0x17C6C64U), v1);
        M.WU32(0x17C6DB4U, v1);
        if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
        F_4CEE10((3));
        v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, ((uint)(v2 + (uint)(((1)) * 4))));
        F_4CEE70();
        goto L6;
        L5: ;
        v2 = ((uint)((0)));
        L6: ;
        F_4C12B0();
        F_4CAFF0();
        if (!(v2 != 0)) goto L7;
        M.WU32(v2, M.U32(M.U32(0x17C6C64U)));
        L7: ;
        goto L4;
        L3: ;
        M.WU32(M.U32(0x17C6C64U), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
        L4: ;
        L1: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C1860  void __usercall(int result@<eax>)
    void F_4C1860(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = result;
        v2 = ((int)(((uint)((4))) * ((uint)(result))));
        if (!(((int)(((uint)((4))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6C64U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6C64U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6C64U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6C64U, v3);
        F_4C17B0();
        v3 = ((uint)(v3 + (uint)(((4)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6C64U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4C18C0  int()
    int F_4C18C0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6918U);
        M.WU32(M.U32(0x17C6918U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6918U), ((uint)((12))));
        v0 = M.U32(0x17C6918U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((12)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6918U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((2)) * 4)))) != 0)) goto L7;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), v3);
        M.WU32(0x17C6C64U, v3);
        F_4C1860(((int)(M.U32(((uint)(v0 + (uint)(((1)) * 4)))))));
        L7: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C1980  int()
    int F_4C1980()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6960U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6960U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6960U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6960U))), v2);
            M.WU32(0x17C6918U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4C18C0();
            v4 = ((uint)(M.I32(0x17C6960U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6960U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((25))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6960U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4C1EA0  int()
    int F_4C1EA0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        int v6 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6954U);
        M.WU32(M.U32(0x17C6954U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6954U), ((uint)((8))));
        v0 = M.U32(0x17C6954U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((8)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6954U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        v3 = M.U32(((uint)(v0 + (uint)(((1)) * 4))));
        v4 = ((uint)(v0 + (uint)(((1)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v0 + (uint)(((1)) * 4))));
        if (!(v3 != 0)) goto L7;
        if (!(v3 == ((uint)((-1))))) goto L9;
        v5 = M.U32(0x1EABC3CU);
        M.WU32(v4, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v5)));
        Nat.Load_XStringData(v4);
        goto L10;
        L9: ;
        M.WU32(v4, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v3 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v3 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L10: ;
        L7: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C1F80  int()
    int F_4C1F80()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6FA8U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6FA8U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6FA8U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6FA8U))), v2);
            M.WU32(0x17C6954U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4C1EA0();
            v4 = ((uint)(M.I32(0x17C6FA8U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6FA8U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((27))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6FA8U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4C20C0  int()
    int F_4C20C0()
    {
        uint v0 = 0;
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        int result = 0;
        byte v6 = 0;
        int v7 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        Nat.Memset(M.U32(0x17C6CFCU), ((uint)((0))), ((uint)((2240))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6CFCU), ((uint)((2240))));
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((2240)) * 1))));
        v0 = M.U32(0x17C6CFCU);
        v7 = (16);
        L3: ;
        M.WU32(0x17C6CFCU, v0);
        v1 = ((int)(v0));
        v2 = (31);
        L6: ;
        M.WI32(0x17C6B88U, v1);
        F_4BCDF0();
        v1 = ((int)(((uint)(v1)) + ((uint)((4)))));
        Rt::PreAdd(v2, -1);
        L7: ;
        if ((v2 != 0)) goto L6;
        L8: ;
        v3 = ((uint)(M.U32(0x17C6CFCU) + (uint)(((124)) * 1)));
        v4 = (4);
        L9: ;
        M.WI32(0x17C6B88U, ((int)(v3)));
        result = F_4BCDF0();
        v3 = ((uint)(v3 + (uint)(((4)) * 1)));
        Rt::PreAdd(v4, -1);
        L10: ;
        if ((v4 != 0)) goto L9;
        L11: ;
        v0 = ((uint)(v0 + (uint)(((140)) * 1)));
        v6 = ((((uint)(Rt::PostAdd(v7, -1))) == ((uint)((1)))) ? (byte)1 : (byte)0);
        M.WI32(0x17C6B88U, ((int)(((uint)(v3 - (uint)(((16)) * 1))))));
        L4: ;
        if (!((v6 != 0))) goto L3;
        L5: ;
        M.WU32(0x17C6CFCU, ((uint)(v0 - (uint)(((2240)) * 1))));
        return result;
        return {};
    }

    // sub_4C2180  int()
    int F_4C2180()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6F18U);
        M.WU32(M.U32(0x17C6F18U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6F18U), ((uint)((8))));
        v0 = M.U32(0x17C6F18U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((8)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6F18U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((1)) * 4)))) != 0)) goto L7;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), v3);
        M.WU32(0x17C6CFCU, v3);
        F_4C20C0();
        L7: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C2230  int()
    int F_4C2230()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint inserted = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6A40U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6A40U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6A40U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6A40U))), v2);
            M.WU32(0x17C6F18U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            inserted = F_4CEEC0();
            goto L6;
            L5: ;
            inserted = ((uint)((0)));
            L6: ;
            F_4C2180();
            v4 = ((uint)(M.I32(0x17C6A40U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6A40U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((31))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(inserted != 0)) goto L7;
            M.WU32(inserted, M.U32(((uint)(M.I32(0x17C6A40U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4C23D0  int()
    int F_4C23D0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        int v3 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6F30U);
        Nat.Memset(M.U32(0x17C6F30U), ((uint)((0))), ((uint)((112))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6F30U), ((uint)((112))));
        v0 = M.U32(0x17C6F30U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((112)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!!((v1 != 0))) goto L3;
        goto LBL_9;
        L3: ;
        if (!(v1 != ((uint)((-1))))) goto L5;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        LBL_9: ;
        M.WU32(0x17C698CU, ((uint)(v0 + (uint)(((1)) * 4))));
        F_4BA420();
        return F_4CEE70();
        L5: ;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6F30U) + (uint)(((1)) * 4))));
        F_4BA420();
        return F_4CEE70();
        return {};
    }

    // sub_4C2490  int()
    int F_4C2490()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6F2CU))))));
            if (!(M.U32(((uint)(M.I32(0x17C6F2CU)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6F2CU))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6F2CU))), v2);
            M.WU32(0x17C6F30U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4C23D0();
            v4 = ((uint)(M.I32(0x17C6F2CU)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6F2CU)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((40))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6F2CU)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4C2630  int()
    int F_4C2630()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        uint v6 = 0;
        uint v7 = 0;
        uint v8 = 0;
        uint v9 = 0;
        uint v10 = 0;
        uint v11 = 0;
        uint v12 = 0;
        int v13 = 0;
        uint v14 = 0;
        int v15 = 0;
        uint v16 = 0;
        int v17 = 0;
        uint v18 = 0;
        int v19 = 0;
        uint v20 = 0;
        int v21 = 0;
        uint v22 = 0;
        int v23 = 0;
        uint v24 = 0;
        int v25 = 0;
        uint v26 = 0;
        int v27 = 0;
        uint v28 = 0;
        int v29 = 0;
        byte v30 = 0;
        uint v31 = 0;
        uint v32 = 0;
        uint v33 = 0;
        uint v34 = 0;
        uint v35 = 0;
        uint v36 = 0;
        uint v37 = 0;
        uint v38 = 0;
        uint v39 = 0;
        uint v40 = 0;
        uint v41 = 0;
        uint v42 = 0;
        uint v43 = 0;
        uint v44 = 0;
        uint v45 = 0;
        uint v46 = 0;
        uint v47 = 0;
        uint v48 = 0;
        uint v49 = 0;
        uint v50 = 0;
        uint v51 = 0;
        uint v52 = 0;
        uint v53 = 0;
        uint v54 = 0;
        uint v55 = 0;
        uint v56 = 0;
        uint v57 = 0;
        uint v58 = 0;
        uint v59 = 0;
        uint v60 = 0;
        uint v61 = 0;
        uint v62 = 0;
        uint v63 = 0;
        uint v64 = 0;
        uint v65 = 0;
        uint v66 = 0;
        uint v67 = 0;
        uint v68 = 0;
        uint v69 = 0;
        uint v70 = 0;
        uint v71 = 0;
        uint v72 = 0;
        uint v73 = 0;
        uint v74 = 0;
        uint v75 = 0;
        uint v76 = 0;
        uint v77 = 0;
        uint v78 = 0;
        uint v79 = 0;
        uint v80 = 0;
        uint v81 = 0;
        uint v82 = 0;
        uint v83 = 0;
        uint v84 = 0;
        uint v85 = 0;
        uint v86 = 0;
        uint v87 = 0;
        uint v88 = 0;
        uint v89 = 0;
        uint v90 = 0;
        int v91 = 0;
        uint v92 = 0;
        uint v93 = 0;
        uint v94 = 0;
        int v95 = 0;
        uint v96 = 0;
        uint v97 = 0;
        int v98 = 0;
        int v99 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6A78U);
        Nat.Memset(M.U32(0x17C6A78U), ((uint)((0))), ((uint)((3068))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A78U), ((uint)((3068))));
        v0 = M.U32(0x17C6A78U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((3068)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6A78U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        v3 = M.U32(((uint)(v0 + (uint)(((1)) * 4))));
        v4 = ((uint)(v0 + (uint)(((1)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v0 + (uint)(((1)) * 4))));
        if (!(v3 != 0)) goto L7;
        if (!(v3 == ((uint)((-1))))) goto L9;
        v5 = M.U32(0x1EABC3CU);
        M.WU32(v4, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v5)));
        Nat.Load_XStringData(v4);
        v0 = M.U32(0x17C6A78U);
        goto L10;
        L9: ;
        M.WU32(v4, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v3 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v3 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L10: ;
        L7: ;
        v6 = M.U32(((uint)(v0 + (uint)(((2)) * 4))));
        v7 = ((uint)(v0 + (uint)(((2)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v0 + (uint)(((2)) * 4))));
        if (!(v6 != 0)) goto L11;
        if (!(v6 == ((uint)((-1))))) goto L13;
        v8 = M.U32(0x1EABC3CU);
        M.WU32(v7, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v8)));
        Nat.Load_XStringData(v7);
        v0 = M.U32(0x17C6A78U);
        goto L14;
        L13: ;
        M.WU32(v7, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v6 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v6 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L14: ;
        L11: ;
        v9 = M.U32(((uint)(v0 + (uint)(((3)) * 4))));
        v10 = ((uint)(v0 + (uint)(((3)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v0 + (uint)(((3)) * 4))));
        if (!(v9 != 0)) goto L15;
        if (!(v9 == ((uint)((-1))))) goto L17;
        v11 = M.U32(0x1EABC3CU);
        M.WU32(v10, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v11)));
        Nat.Load_XStringData(v10);
        v0 = M.U32(0x17C6A78U);
        goto L18;
        L17: ;
        M.WU32(v10, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v9 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v9 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L18: ;
        L15: ;
        M.WI32(0x17C69D4U, ((int)(((uint)(v0 + (uint)(((4)) * 4))))));
        F_4BBEF0();
        v12 = ((uint)(M.U32(0x17C6A78U) + (uint)(((5)) * 4)));
        v13 = (2);
        L19: ;
        M.WI32(0x17C69D4U, ((int)(v12)));
        F_4BBEF0();
        Rt::PreAdd(v12, 4);
        Rt::PreAdd(v13, -1);
        L20: ;
        if ((v13 != 0)) goto L19;
        L21: ;
        v14 = ((uint)(M.U32(0x17C6A78U) + (uint)(((7)) * 4)));
        v15 = (2);
        L22: ;
        M.WI32(0x17C69D4U, ((int)(v14)));
        F_4BBEF0();
        Rt::PreAdd(v14, 4);
        Rt::PreAdd(v15, -1);
        L23: ;
        if ((v15 != 0)) goto L22;
        L24: ;
        M.WI32(0x17C69D4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((9)) * 4))))));
        F_4BBEF0();
        M.WI32(0x17C69D4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((10)) * 4))))));
        F_4BBEF0();
        M.WI32(0x17C69D4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((11)) * 4))))));
        F_4BBEF0();
        M.WI32(0x17C69D4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((12)) * 4))))));
        F_4BBEF0();
        M.WI32(0x17C69D4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((13)) * 4))))));
        F_4BBEF0();
        M.WI32(0x17C69D4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((14)) * 4))))));
        F_4BBEF0();
        v16 = M.U32(0x17C6A78U);
        v17 = M.I32(0x17C6B6CU);
        v18 = ((uint)(M.U32(0x17C6A78U) + (uint)(((15)) * 4)));
        v19 = (32);
        L25: ;
        M.WU16(v18, M.U16(((uint)(((int)(M.U32(((uint)(((int)(((uint)(v17)) + ((uint)((4)))))))) + ((uint)(((int)(((uint)((4))) * ((uint)(M.U16(v18)))))))))))));
        v18 = ((uint)(v18 + (uint)(((2)) * 1)));
        Rt::PreAdd(v19, -1);
        L26: ;
        if ((v19 != 0)) goto L25;
        L27: ;
        M.WU32(0x17C6F3CU, ((uint)(v18 - (uint)(((16)) * 4))));
        M.WU32(0x17C6EECU, ((uint)(v16 + (uint)(((31)) * 4))));
        F_4B70F0((48));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6A78U) + (uint)(((79)) * 4))));
        F_4B70F0((48));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6A78U) + (uint)(((127)) * 4))));
        F_4B70F0((48));
        v20 = M.U32(0x17C6A78U);
        v21 = M.I32(0x17C6B6CU);
        v22 = ((uint)(M.U32(0x17C6A78U) + (uint)(((175)) * 4)));
        v23 = (16);
        L28: ;
        M.WU16(v22, M.U16(((uint)(((int)(M.U32(((uint)(((int)(((uint)(v21)) + ((uint)((4)))))))) + ((uint)(((int)(((uint)((4))) * ((uint)(M.U16(v22)))))))))))));
        v22 = ((uint)(v22 + (uint)(((2)) * 1)));
        Rt::PreAdd(v23, -1);
        L29: ;
        if ((v23 != 0)) goto L28;
        L30: ;
        v24 = ((uint)(v20 + (uint)(((183)) * 4)));
        v25 = (16);
        L31: ;
        M.WU16(v24, M.U16(((uint)(((int)(M.U32(((uint)(((int)(((uint)(v21)) + ((uint)((4)))))))) + ((uint)(((int)(((uint)((4))) * ((uint)(M.U16(v24)))))))))))));
        Rt::PreAdd(v24, 2);
        Rt::PreAdd(v25, -1);
        L32: ;
        if ((v25 != 0)) goto L31;
        L33: ;
        v26 = ((uint)(v20 + (uint)(((191)) * 4)));
        v27 = (16);
        L34: ;
        M.WU16(v26, M.U16(((uint)(((int)(M.U32(((uint)(((int)(((uint)(v21)) + ((uint)((4)))))))) + ((uint)(((int)(((uint)((4))) * ((uint)(M.U16(v26)))))))))))));
        Rt::PreAdd(v26, 2);
        Rt::PreAdd(v27, -1);
        L35: ;
        if ((v27 != 0)) goto L34;
        L36: ;
        v28 = ((uint)(v20 + (uint)(((199)) * 4)));
        v29 = (16);
        L37: ;
        M.WU16(v28, M.U16(((uint)(((int)(M.U32(((uint)(((int)(((uint)(v21)) + ((uint)((4)))))))) + ((uint)(((int)(((uint)((4))) * ((uint)(M.U16(v28)))))))))))));
        Rt::PreAdd(v28, 2);
        Rt::PreAdd(v29, -1);
        L38: ;
        if ((v29 != 0)) goto L37;
        L39: ;
        M.WU32(0x17C6F3CU, ((uint)(v28 - (uint)(((16)) * 2))));
        M.WI32(0x17C6B88U, ((int)(((uint)(v20 + (uint)(((215)) * 4))))));
        F_4BCDF0();
        M.WI32(0x17C6B88U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((216)) * 4))))));
        F_4BCDF0();
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((217)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((217)) * 4))));
        v31 = ((uint)(M.U32(0x17C6A78U) + (uint)(((217)) * 4)));
        if (!!((v30 != 0))) goto L40;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((217)) * 4))))));
        F_4B7160();
        M.WU32(v31, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L40: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((218)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((218)) * 4))));
        v32 = ((uint)(M.U32(0x17C6A78U) + (uint)(((218)) * 4)));
        if (!!((v30 != 0))) goto L42;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((218)) * 4))))));
        F_4B7160();
        M.WU32(v32, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L42: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((219)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((219)) * 4))));
        v33 = ((uint)(M.U32(0x17C6A78U) + (uint)(((219)) * 4)));
        if (!!((v30 != 0))) goto L44;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((219)) * 4))))));
        F_4B7160();
        M.WU32(v33, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L44: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((220)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((220)) * 4))));
        v34 = ((uint)(M.U32(0x17C6A78U) + (uint)(((220)) * 4)));
        if (!!((v30 != 0))) goto L46;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((220)) * 4))))));
        F_4B7160();
        M.WU32(v34, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L46: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((221)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((221)) * 4))));
        v35 = ((uint)(M.U32(0x17C6A78U) + (uint)(((221)) * 4)));
        if (!!((v30 != 0))) goto L48;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((221)) * 4))))));
        F_4B7160();
        M.WU32(v35, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L48: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((222)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((222)) * 4))));
        v36 = ((uint)(M.U32(0x17C6A78U) + (uint)(((222)) * 4)));
        if (!!((v30 != 0))) goto L50;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((222)) * 4))))));
        F_4B7160();
        M.WU32(v36, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L50: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((223)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((223)) * 4))));
        v37 = ((uint)(M.U32(0x17C6A78U) + (uint)(((223)) * 4)));
        if (!!((v30 != 0))) goto L52;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((223)) * 4))))));
        F_4B7160();
        M.WU32(v37, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L52: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((224)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((224)) * 4))));
        v38 = ((uint)(M.U32(0x17C6A78U) + (uint)(((224)) * 4)));
        if (!!((v30 != 0))) goto L54;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((224)) * 4))))));
        F_4B7160();
        M.WU32(v38, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L54: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((225)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((225)) * 4))));
        v39 = ((uint)(M.U32(0x17C6A78U) + (uint)(((225)) * 4)));
        if (!!((v30 != 0))) goto L56;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((225)) * 4))))));
        F_4B7160();
        M.WU32(v39, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L56: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((226)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((226)) * 4))));
        v40 = ((uint)(M.U32(0x17C6A78U) + (uint)(((226)) * 4)));
        if (!!((v30 != 0))) goto L58;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((226)) * 4))))));
        F_4B7160();
        M.WU32(v40, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L58: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((227)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((227)) * 4))));
        v41 = ((uint)(M.U32(0x17C6A78U) + (uint)(((227)) * 4)));
        if (!!((v30 != 0))) goto L60;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((227)) * 4))))));
        F_4B7160();
        M.WU32(v41, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L60: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((228)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((228)) * 4))));
        v42 = ((uint)(M.U32(0x17C6A78U) + (uint)(((228)) * 4)));
        if (!!((v30 != 0))) goto L62;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((228)) * 4))))));
        F_4B7160();
        M.WU32(v42, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L62: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((229)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((229)) * 4))));
        v43 = ((uint)(M.U32(0x17C6A78U) + (uint)(((229)) * 4)));
        if (!!((v30 != 0))) goto L64;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((229)) * 4))))));
        F_4B7160();
        M.WU32(v43, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L64: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((230)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((230)) * 4))));
        v44 = ((uint)(M.U32(0x17C6A78U) + (uint)(((230)) * 4)));
        if (!!((v30 != 0))) goto L66;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((230)) * 4))))));
        F_4B7160();
        M.WU32(v44, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L66: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((231)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((231)) * 4))));
        v45 = ((uint)(M.U32(0x17C6A78U) + (uint)(((231)) * 4)));
        if (!!((v30 != 0))) goto L68;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((231)) * 4))))));
        F_4B7160();
        M.WU32(v45, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L68: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((232)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((232)) * 4))));
        v46 = ((uint)(M.U32(0x17C6A78U) + (uint)(((232)) * 4)));
        if (!!((v30 != 0))) goto L70;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((232)) * 4))))));
        F_4B7160();
        M.WU32(v46, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L70: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((233)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((233)) * 4))));
        v47 = ((uint)(M.U32(0x17C6A78U) + (uint)(((233)) * 4)));
        if (!!((v30 != 0))) goto L72;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((233)) * 4))))));
        F_4B7160();
        M.WU32(v47, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L72: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((234)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((234)) * 4))));
        v48 = ((uint)(M.U32(0x17C6A78U) + (uint)(((234)) * 4)));
        if (!!((v30 != 0))) goto L74;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((234)) * 4))))));
        F_4B7160();
        M.WU32(v48, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L74: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((235)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((235)) * 4))));
        v49 = ((uint)(M.U32(0x17C6A78U) + (uint)(((235)) * 4)));
        if (!!((v30 != 0))) goto L76;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((235)) * 4))))));
        F_4B7160();
        M.WU32(v49, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L76: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((236)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((236)) * 4))));
        v50 = ((uint)(M.U32(0x17C6A78U) + (uint)(((236)) * 4)));
        if (!!((v30 != 0))) goto L78;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((236)) * 4))))));
        F_4B7160();
        M.WU32(v50, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L78: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((237)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((237)) * 4))));
        v51 = ((uint)(M.U32(0x17C6A78U) + (uint)(((237)) * 4)));
        if (!!((v30 != 0))) goto L80;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((237)) * 4))))));
        F_4B7160();
        M.WU32(v51, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L80: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((238)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((238)) * 4))));
        v52 = ((uint)(M.U32(0x17C6A78U) + (uint)(((238)) * 4)));
        if (!!((v30 != 0))) goto L82;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((238)) * 4))))));
        F_4B7160();
        M.WU32(v52, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L82: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((239)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((239)) * 4))));
        v53 = ((uint)(M.U32(0x17C6A78U) + (uint)(((239)) * 4)));
        if (!!((v30 != 0))) goto L84;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((239)) * 4))))));
        F_4B7160();
        M.WU32(v53, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L84: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((240)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((240)) * 4))));
        v54 = ((uint)(M.U32(0x17C6A78U) + (uint)(((240)) * 4)));
        if (!!((v30 != 0))) goto L86;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((240)) * 4))))));
        F_4B7160();
        M.WU32(v54, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L86: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((241)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((241)) * 4))));
        v55 = ((uint)(M.U32(0x17C6A78U) + (uint)(((241)) * 4)));
        if (!!((v30 != 0))) goto L88;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((241)) * 4))))));
        F_4B7160();
        M.WU32(v55, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L88: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((242)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((242)) * 4))));
        v56 = ((uint)(M.U32(0x17C6A78U) + (uint)(((242)) * 4)));
        if (!!((v30 != 0))) goto L90;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((242)) * 4))))));
        F_4B7160();
        M.WU32(v56, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L90: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((243)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((243)) * 4))));
        v57 = ((uint)(M.U32(0x17C6A78U) + (uint)(((243)) * 4)));
        if (!!((v30 != 0))) goto L92;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((243)) * 4))))));
        F_4B7160();
        M.WU32(v57, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L92: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((244)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((244)) * 4))));
        v58 = ((uint)(M.U32(0x17C6A78U) + (uint)(((244)) * 4)));
        if (!!((v30 != 0))) goto L94;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((244)) * 4))))));
        F_4B7160();
        M.WU32(v58, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L94: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((245)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((245)) * 4))));
        v59 = ((uint)(M.U32(0x17C6A78U) + (uint)(((245)) * 4)));
        if (!!((v30 != 0))) goto L96;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((245)) * 4))))));
        F_4B7160();
        M.WU32(v59, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L96: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((246)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((246)) * 4))));
        v60 = ((uint)(M.U32(0x17C6A78U) + (uint)(((246)) * 4)));
        if (!!((v30 != 0))) goto L98;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((246)) * 4))))));
        F_4B7160();
        M.WU32(v60, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L98: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((247)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((247)) * 4))));
        v61 = ((uint)(M.U32(0x17C6A78U) + (uint)(((247)) * 4)));
        if (!!((v30 != 0))) goto L100;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((247)) * 4))))));
        F_4B7160();
        M.WU32(v61, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L100: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((248)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((248)) * 4))));
        v62 = ((uint)(M.U32(0x17C6A78U) + (uint)(((248)) * 4)));
        if (!!((v30 != 0))) goto L102;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((248)) * 4))))));
        F_4B7160();
        M.WU32(v62, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L102: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((249)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((249)) * 4))));
        v63 = ((uint)(M.U32(0x17C6A78U) + (uint)(((249)) * 4)));
        if (!!((v30 != 0))) goto L104;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((249)) * 4))))));
        F_4B7160();
        M.WU32(v63, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L104: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((250)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((250)) * 4))));
        v64 = ((uint)(M.U32(0x17C6A78U) + (uint)(((250)) * 4)));
        if (!!((v30 != 0))) goto L106;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((250)) * 4))))));
        F_4B7160();
        M.WU32(v64, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L106: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((251)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((251)) * 4))));
        v65 = ((uint)(M.U32(0x17C6A78U) + (uint)(((251)) * 4)));
        if (!!((v30 != 0))) goto L108;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((251)) * 4))))));
        F_4B7160();
        M.WU32(v65, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L108: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((252)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((252)) * 4))));
        v66 = ((uint)(M.U32(0x17C6A78U) + (uint)(((252)) * 4)));
        if (!!((v30 != 0))) goto L110;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((252)) * 4))))));
        F_4B7160();
        M.WU32(v66, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L110: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((253)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((253)) * 4))));
        v67 = ((uint)(M.U32(0x17C6A78U) + (uint)(((253)) * 4)));
        if (!!((v30 != 0))) goto L112;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((253)) * 4))))));
        F_4B7160();
        M.WU32(v67, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L112: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((254)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((254)) * 4))));
        v68 = ((uint)(M.U32(0x17C6A78U) + (uint)(((254)) * 4)));
        if (!!((v30 != 0))) goto L114;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((254)) * 4))))));
        F_4B7160();
        M.WU32(v68, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L114: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((255)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((255)) * 4))));
        v69 = ((uint)(M.U32(0x17C6A78U) + (uint)(((255)) * 4)));
        if (!!((v30 != 0))) goto L116;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((255)) * 4))))));
        F_4B7160();
        M.WU32(v69, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L116: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((256)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((256)) * 4))));
        v70 = ((uint)(M.U32(0x17C6A78U) + (uint)(((256)) * 4)));
        if (!!((v30 != 0))) goto L118;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((256)) * 4))))));
        F_4B7160();
        M.WU32(v70, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L118: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((257)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((257)) * 4))));
        v71 = ((uint)(M.U32(0x17C6A78U) + (uint)(((257)) * 4)));
        if (!!((v30 != 0))) goto L120;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((257)) * 4))))));
        F_4B7160();
        M.WU32(v71, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L120: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((258)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((258)) * 4))));
        v72 = ((uint)(M.U32(0x17C6A78U) + (uint)(((258)) * 4)));
        if (!!((v30 != 0))) goto L122;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((258)) * 4))))));
        F_4B7160();
        M.WU32(v72, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L122: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((259)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((259)) * 4))));
        v73 = ((uint)(M.U32(0x17C6A78U) + (uint)(((259)) * 4)));
        if (!!((v30 != 0))) goto L124;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((259)) * 4))))));
        F_4B7160();
        M.WU32(v73, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L124: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((260)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((260)) * 4))));
        v74 = ((uint)(M.U32(0x17C6A78U) + (uint)(((260)) * 4)));
        if (!!((v30 != 0))) goto L126;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((260)) * 4))))));
        F_4B7160();
        M.WU32(v74, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L126: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((261)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((261)) * 4))));
        v75 = ((uint)(M.U32(0x17C6A78U) + (uint)(((261)) * 4)));
        if (!!((v30 != 0))) goto L128;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((261)) * 4))))));
        F_4B7160();
        M.WU32(v75, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L128: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((262)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((262)) * 4))));
        v76 = ((uint)(M.U32(0x17C6A78U) + (uint)(((262)) * 4)));
        if (!!((v30 != 0))) goto L130;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((262)) * 4))))));
        F_4B7160();
        M.WU32(v76, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L130: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((263)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((263)) * 4))));
        v77 = ((uint)(M.U32(0x17C6A78U) + (uint)(((263)) * 4)));
        if (!!((v30 != 0))) goto L132;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((263)) * 4))))));
        F_4B7160();
        M.WU32(v77, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L132: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((264)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((264)) * 4))));
        v78 = ((uint)(M.U32(0x17C6A78U) + (uint)(((264)) * 4)));
        if (!!((v30 != 0))) goto L134;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((264)) * 4))))));
        F_4B7160();
        M.WU32(v78, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L134: ;
        v30 = ((M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((265)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((265)) * 4))));
        v79 = ((uint)(M.U32(0x17C6A78U) + (uint)(((265)) * 4)));
        if (!!((v30 != 0))) goto L136;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((265)) * 4))))));
        F_4B7160();
        M.WU32(v79, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L136: ;
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((266)) * 4))));
        F_61E860();
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((267)) * 4))));
        F_61E860();
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((268)) * 4))));
        F_61E860();
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((269)) * 4))));
        F_61E860();
        v80 = M.U32(0x17C6A78U);
        v81 = M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((270)) * 4))));
        if (!(v81 != 0)) goto L138;
        if (!(v81 == ((uint)((-1))))) goto L140;
        v82 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(M.U32(0x17C6A78U) + (uint)(((270)) * 4))), v82);
        M.WU32(0x1EABC3CU, v82);
        M.WU32(0x17C6CDCU, v82);
        F_4B8920((31), ((sbyte)((1))));
        v80 = M.U32(0x17C6A78U);
        goto L141;
        L140: ;
        M.WU32(((uint)(M.U32(0x17C6A78U) + (uint)(((270)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v81 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v81 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L141: ;
        L138: ;
        v83 = M.U32(((uint)(v80 + (uint)(((271)) * 4))));
        if (!(v83 != 0)) goto L142;
        if (!(v83 == ((uint)((-1))))) goto L144;
        v84 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(v80 + (uint)(((271)) * 4))), v84);
        M.WU32(0x1EABC3CU, v84);
        M.WU32(0x17C6CDCU, v84);
        F_4B8920((31), ((sbyte)((1))));
        v80 = M.U32(0x17C6A78U);
        goto L145;
        L144: ;
        M.WU32(((uint)(v80 + (uint)(((271)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v83 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v83 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L145: ;
        L142: ;
        M.WI32(0x17C6B88U, ((int)(((uint)(v80 + (uint)(((272)) * 4))))));
        F_4BCDF0();
        M.WI32(0x17C6B88U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((273)) * 4))))));
        F_4BCDF0();
        M.WI32(0x17C6B88U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((274)) * 4))))));
        F_4BCDF0();
        M.WI32(0x17C6B88U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((275)) * 4))))));
        F_4BCDF0();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((276)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((277)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((331)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((332)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((333)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((334)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((335)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((336)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((338)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((340)) * 4))));
        F_4BA420();
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6A78U) + (uint)(((345)) * 4))));
        F_4B7070(((sbyte)((0))));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6A78U) + (uint)(((347)) * 4))));
        F_4B7070(((sbyte)((0))));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6A78U) + (uint)(((351)) * 4))));
        F_4B7070(((sbyte)((0))));
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((417)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((418)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((419)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((420)) * 4))));
        F_4BA420();
        M.WI32(0x17C6F6CU, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((470)) * 4))))));
        F_4BB710();
        M.WI32(0x17C69D4U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((492)) * 4))))));
        F_4BBEF0();
        M.WI32(0x17C6B88U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((494)) * 4))))));
        F_4BCDF0();
        M.WI32(0x17C6B88U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((495)) * 4))))));
        F_4BCDF0();
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((496)) * 4))));
        F_61E860();
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((497)) * 4))));
        F_61E860();
        M.WI32(0x17C6B88U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((563)) * 4))))));
        F_4BCDF0();
        M.WI32(0x17C6B88U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((564)) * 4))))));
        F_4BCDF0();
        M.WI32(0x17C6B88U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((571)) * 4))))));
        F_4BCDF0();
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((572)) * 4))));
        F_61E860();
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6A78U) + (uint)(((616)) * 4))));
        F_4B7070(((sbyte)((0))));
        v85 = M.U32(0x17C6A78U);
        v86 = M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((618)) * 4))));
        if (!(v86 != 0)) goto L146;
        if (!(v86 == ((uint)((-1))))) goto L148;
        v87 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v87);
        M.WU32(((uint)(M.U32(0x17C6A78U) + (uint)(((618)) * 4))), v87);
        M.WU32(0x17C6A48U, v87);
        F_4B6D50(((int)(M.U16(((uint)(v85 + (uint)(((1496)) * 2)))))));
        v85 = M.U32(0x17C6A78U);
        goto L149;
        L148: ;
        M.WU32(((uint)(M.U32(0x17C6A78U) + (uint)(((618)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v86 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v86 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L149: ;
        L146: ;
        M.WU32(0x17C6EECU, ((uint)(v85 + (uint)(((617)) * 4))));
        F_4B7070(((sbyte)((0))));
        v88 = M.U32(0x17C6A78U);
        v89 = M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((619)) * 4))));
        if (!(v89 != 0)) goto L150;
        if (!(v89 == ((uint)((-1))))) goto L152;
        v90 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(M.U32(0x17C6A78U) + (uint)(((619)) * 4))), v90);
        v91 = ((int)(M.U16(((uint)(v88 + (uint)(((1497)) * 2))))));
        M.WU32(0x1EABC3CU, v90);
        M.WU32(0x17C6A48U, v90);
        F_4B6D50(v91);
        v88 = M.U32(0x17C6A78U);
        goto L153;
        L152: ;
        M.WU32(((uint)(M.U32(0x17C6A78U) + (uint)(((619)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v89 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v89 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L153: ;
        L150: ;
        M.WU32(0x17C6EECU, ((uint)(v88 + (uint)(((639)) * 4))));
        F_4B7070(((sbyte)((0))));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6A78U) + (uint)(((640)) * 4))));
        F_4B7070(((sbyte)((0))));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6A78U) + (uint)(((648)) * 4))));
        F_4B7070(((sbyte)((0))));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6A78U) + (uint)(((649)) * 4))));
        F_4B7070(((sbyte)((0))));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6A78U) + (uint)(((680)) * 4))));
        F_4B7070(((sbyte)((0))));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6A78U) + (uint)(((681)) * 4))));
        F_4B7070(((sbyte)((0))));
        M.WI32(0x17C6F2CU, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((682)) * 4))))));
        F_4C2490();
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((689)) * 4))));
        F_61E860();
        M.WI32(0x17C6B88U, ((int)(((uint)(M.U32(0x17C6A78U) + (uint)(((690)) * 4))))));
        F_4BCDF0();
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6A78U) + (uint)(((691)) * 4))));
        F_4B7070(((sbyte)((0))));
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((695)) * 4))));
        F_61E860();
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((696)) * 4))));
        F_4B8920((4), ((sbyte)((0))));
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((700)) * 4))));
        F_4B8920((4), ((sbyte)((0))));
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((704)) * 4))));
        F_61E860();
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((705)) * 4))));
        F_61E860();
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((722)) * 4))));
        F_61E860();
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((723)) * 4))));
        F_61E860();
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((724)) * 4))));
        F_61E860();
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((725)) * 4))));
        F_61E860();
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((726)) * 4))));
        F_61E860();
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6A78U) + (uint)(((727)) * 4))));
        F_61E860();
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6A78U) + (uint)(((734)) * 4))));
        F_4B7070(((sbyte)((0))));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6A78U) + (uint)(((737)) * 4))));
        F_4B7070(((sbyte)((0))));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6A78U) + (uint)(((738)) * 4))));
        F_4B7070(((sbyte)((0))));
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((740)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((741)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A78U) + (uint)(((742)) * 4))));
        F_4BA420();
        v92 = M.U32(0x17C6A78U);
        v93 = M.U32(((uint)(M.U32(0x17C6A78U) + (uint)(((749)) * 4))));
        if (!(v93 != 0)) goto L154;
        if (!(v93 == ((uint)((-1))))) goto L156;
        v94 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(M.U32(0x17C6A78U) + (uint)(((749)) * 4))), v94);
        v95 = ((int)(M.U16(((uint)(v92 + (uint)(((1496)) * 2))))));
        M.WU32(0x1EABC3CU, v94);
        M.WU32(0x17C6A48U, v94);
        F_4B6D50(v95);
        v92 = M.U32(0x17C6A78U);
        goto L157;
        L156: ;
        M.WU32(((uint)(M.U32(0x17C6A78U) + (uint)(((749)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v93 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v93 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L157: ;
        L154: ;
        v96 = M.U32(((uint)(v92 + (uint)(((750)) * 4))));
        if (!(v96 != 0)) goto L158;
        if (!(v96 == ((uint)((-1))))) goto L160;
        v97 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(v92 + (uint)(((750)) * 4))), v97);
        v98 = ((int)(M.U16(((uint)(v92 + (uint)(((1497)) * 2))))));
        M.WU32(0x1EABC3CU, v97);
        M.WU32(0x17C6A48U, v97);
        F_4B6D50(v98);
        goto L161;
        L160: ;
        M.WU32(((uint)(v92 + (uint)(((750)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v96 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v96 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L161: ;
        L158: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C3C10  int()
    int F_4C3C10()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6DDCU))))));
            if (!(M.U32(((uint)(M.I32(0x17C6DDCU)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6DDCU))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6DDCU))), v2);
            M.WU32(0x17C6A78U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4C2630();
            v4 = ((uint)(M.I32(0x17C6DDCU)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6DDCU)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((28))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6DDCU)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4C3D00  void __cdecl(int a1)
    void F_4C3D00(int a1)
    {
        int v1 = 0;
        byte v2 = 0;
        int v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        int v6 = 0;
        uint v7 = 0;
        uint v8 = 0;
        int v9 = 0;
        v1 = a1;
        v2 = ((((uint)(((int)(((uint)((12))) * ((uint)(a1)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v3 = ((int)(((uint)((12))) * ((uint)(a1))));
        v9 = ((int)(((uint)((12))) * ((uint)(a1))));
        if (!!((v2 != 0))) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6F24U), ((uint)((0))), ((uint)(v3)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6F24U), ((uint)(v3)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v3) * 1))));
        L1: ;
        v4 = M.U32(0x17C6F24U);
        if (!(v1 > (0))) goto L5;
        v5 = ((uint)(M.U32(0x17C6F24U) + (uint)(((8)) * 1)));
        L7: ;
        v6 = ((int)(M.U32(((uint)(v5 - (uint)(((1)) * 4))))));
        M.WU32(0x17C6F24U, v4);
        M.WI32(0x17C692CU, ((int)(v5)));
        if (!((v6 < (16)) || (((uint)(v6)) == ((uint)((39)))))) goto L10;
        v7 = M.U32(v5);
        M.WU32(0x17C6EECU, v5);
        if (!(v7 != 0)) goto L12;
        if (!(v7 == ((uint)((-1))))) goto L14;
        v8 = M.U32(0x1EABC3CU);
        M.WU32(v5, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v8)));
        Nat.Load_XStringData(v5);
        goto L15;
        L14: ;
        M.WU32(v5, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v7 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v7 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L15: ;
        L12: ;
        L10: ;
        v4 = ((uint)(v4 + (uint)(((12)) * 1)));
        v5 = ((uint)(v5 + (uint)(((3)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        v3 = v9;
        L5: ;
        M.WU32(0x17C6F24U, ((uint)(v4 + (uint)((((int)(0 - v3))) * 1))));
    }

    // sub_4C3DD0  int()
    int F_4C3DD0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C69ECU);
        M.WU32(M.U32(0x17C69ECU), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C69ECU), ((uint)((12))));
        v0 = M.U32(0x17C69ECU);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((12)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C69ECU);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((2)) * 4)))) != 0)) goto L7;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), v3);
        M.WU32(0x17C6F24U, v3);
        F_4C3D00(((int)(M.U32(((uint)(v0 + (uint)(((1)) * 4)))))));
        L7: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C3E90  int()
    int F_4C3E90()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6BC0U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6BC0U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6BC0U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6BC0U))), v2);
            M.WU32(0x17C69ECU, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4C3DD0();
            v4 = ((uint)(M.I32(0x17C6BC0U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6BC0U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((43))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6BC0U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4C4E10  char **()
    uint F_4C4E10()
    {
        int v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        int v3 = 0;
        uint result = 0;
        uint v5 = 0;
        v0 = ((int)(M.U32(((uint)(((int)(((uint)(M.I32(0x17C6E04U))) + ((uint)((4))))))))));
        v1 = ((uint)(((int)(((uint)(M.I32(0x17C6E04U))) + ((uint)((4)))))));
        M.WU32(0x17C6EECU, ((uint)(((int)(((uint)(M.I32(0x17C6E04U))) + ((uint)((4))))))));
        if (!(v0 != 0)) goto L1;
        if (!(((uint)(v0)) == ((uint)((-1))))) goto L3;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v1, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v1);
        goto L4;
        L3: ;
        M.WU32(v1, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L4: ;
        L1: ;
        M.WI32(0x17C6F08U, ((int)(((uint)(M.I32(0x17C6E04U))) + ((uint)((8))))));
        F_4BB4D0();
        v3 = ((int)(M.U32(((uint)(((int)(((uint)(M.I32(0x17C6E04U))) + ((uint)((12))))))))));
        result = ((uint)(((int)(((uint)(M.I32(0x17C6E04U))) + ((uint)((12)))))));
        M.WU32(0x17C6EECU, ((uint)(((int)(((uint)(M.I32(0x17C6E04U))) + ((uint)((12))))))));
        if (!(v3 != 0)) goto L5;
        if (!(((uint)(v3)) == ((uint)((-1))))) goto L7;
        v5 = M.U32(0x1EABC3CU);
        M.WU32(result, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v5)));
        return ((uint)(Nat.Load_XStringData(result)));
        goto L8;
        L7: ;
        M.WU32(result, ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v3)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v3)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L8: ;
        L5: ;
        return result;
        return {};
    }

    // sub_4C4EC0  int()
    int F_4C4EC0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        uint v6 = 0;
        uint v7 = 0;
        uint v8 = 0;
        uint v9 = 0;
        byte v10 = 0;
        uint v11 = 0;
        int v12 = 0;
        uint v13 = 0;
        int v14 = 0;
        int v15 = 0;
        uint v16 = 0;
        int v17 = 0;
        uint v18 = 0;
        uint v19 = 0;
        uint v20 = 0;
        uint v21 = 0;
        uint v22 = 0;
        uint v23 = 0;
        uint v24 = 0;
        uint v25 = 0;
        uint v26 = 0;
        uint v27 = 0;
        uint v28 = 0;
        uint v29 = 0;
        uint v30 = 0;
        uint v31 = 0;
        uint v32 = 0;
        uint v33 = 0;
        uint v34 = 0;
        uint v35 = 0;
        int v36 = 0;
        int v37 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6BF4U);
        Nat.Memset(M.U32(0x17C6BF4U), ((uint)((0))), ((uint)((788))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6BF4U), ((uint)((788))));
        v0 = M.U32(0x17C6BF4U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((788)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6BF4U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        v3 = M.U32(((uint)(v0 + (uint)(((2)) * 4))));
        v4 = ((uint)(v0 + (uint)(((2)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v0 + (uint)(((2)) * 4))));
        if (!(v3 != 0)) goto L7;
        if (!(v3 == ((uint)((-1))))) goto L9;
        v5 = M.U32(0x1EABC3CU);
        M.WU32(v4, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v5)));
        Nat.Load_XStringData(v4);
        v0 = M.U32(0x17C6BF4U);
        goto L10;
        L9: ;
        M.WU32(v4, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v3 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v3 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L10: ;
        L7: ;
        M.WI32(0x17C6E04U, ((int)(((uint)(v0 + (uint)(((42)) * 4))))));
        F_4C4E10();
        v6 = M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((100)) * 4))));
        v7 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((100)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((100)) * 4))));
        if (!(v6 != 0)) goto L11;
        if (!(v6 == ((uint)((-1))))) goto L13;
        v8 = M.U32(0x1EABC3CU);
        M.WU32(v7, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v8)));
        Nat.Load_XStringData(v7);
        goto L14;
        L13: ;
        M.WU32(v7, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v6 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v6 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L14: ;
        L11: ;
        M.WI32(0x17C6DDCU, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((101)) * 4))))));
        F_4C3C10();
        v9 = M.U32(0x17C6BF4U);
        v10 = ((M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((107)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((107)) * 4))));
        v11 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((107)) * 4)));
        if (!!((v10 != 0))) goto L15;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((107)) * 4))))));
        F_4B7160();
        v12 = ((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U))))))));
        v9 = M.U32(0x17C6BF4U);
        M.WU32(v11, ((uint)(v12)));
        L15: ;
        v10 = ((M.U32(((uint)(v9 + (uint)(((108)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(v9 + (uint)(((108)) * 4))));
        v13 = ((uint)(v9 + (uint)(((108)) * 4)));
        if (!!((v10 != 0))) goto L17;
        M.WI32(0x17C6DC4U, ((int)(((uint)(v9 + (uint)(((108)) * 4))))));
        F_4B7160();
        v14 = ((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U))))))));
        v9 = M.U32(0x17C6BF4U);
        M.WI32(v13, v14);
        L17: ;
        v15 = M.I32(0x17C6B6CU);
        v16 = ((uint)(v9 + (uint)(((114)) * 4)));
        v17 = (4);
        L19: ;
        M.WU16(v16, M.U16(((uint)(((int)(M.U32(((uint)(((int)(((uint)(v15)) + ((uint)((4)))))))) + ((uint)(((int)(((uint)((4))) * ((uint)(M.U16(v16)))))))))))));
        Rt::PreAdd(v16, 2);
        Rt::PreAdd(v17, -1);
        L20: ;
        if ((v17 != 0)) goto L19;
        L21: ;
        M.WU32(0x17C6F3CU, ((uint)(v16 - (uint)(((4)) * 2))));
        M.WU32(0x17C698CU, ((uint)(v9 + (uint)(((116)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((117)) * 4))));
        F_4BA420();
        v10 = ((M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((120)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((120)) * 4))));
        v18 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((120)) * 4)));
        if (!!((v10 != 0))) goto L22;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((120)) * 4))))));
        F_4B7160();
        M.WU32(v18, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L22: ;
        v10 = ((M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((121)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((121)) * 4))));
        v19 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((121)) * 4)));
        if (!!((v10 != 0))) goto L24;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((121)) * 4))))));
        F_4B7160();
        M.WU32(v19, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L24: ;
        v10 = ((M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((122)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((122)) * 4))));
        v20 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((122)) * 4)));
        if (!!((v10 != 0))) goto L26;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((122)) * 4))))));
        F_4B7160();
        M.WU32(v20, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L26: ;
        v10 = ((M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((123)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((123)) * 4))));
        v21 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((123)) * 4)));
        if (!!((v10 != 0))) goto L28;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((123)) * 4))))));
        F_4B7160();
        M.WU32(v21, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L28: ;
        v10 = ((M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((125)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((125)) * 4))));
        v22 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((125)) * 4)));
        if (!!((v10 != 0))) goto L30;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((125)) * 4))))));
        F_4B7160();
        M.WU32(v22, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L30: ;
        v10 = ((M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((127)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((127)) * 4))));
        v23 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((127)) * 4)));
        if (!!((v10 != 0))) goto L32;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((127)) * 4))))));
        F_4B7160();
        M.WU32(v23, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L32: ;
        v10 = ((M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((128)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((128)) * 4))));
        v24 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((128)) * 4)));
        if (!!((v10 != 0))) goto L34;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((128)) * 4))))));
        F_4B7160();
        M.WU32(v24, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L34: ;
        v10 = ((M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((129)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((129)) * 4))));
        v25 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((129)) * 4)));
        if (!!((v10 != 0))) goto L36;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((129)) * 4))))));
        F_4B7160();
        M.WU32(v25, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L36: ;
        v10 = ((M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((130)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((130)) * 4))));
        v26 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((130)) * 4)));
        if (!!((v10 != 0))) goto L38;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((130)) * 4))))));
        F_4B7160();
        M.WU32(v26, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L38: ;
        v10 = ((M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((132)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((132)) * 4))));
        v27 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((132)) * 4)));
        if (!!((v10 != 0))) goto L40;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((132)) * 4))))));
        F_4B7160();
        M.WU32(v27, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L40: ;
        v10 = ((M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((134)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((134)) * 4))));
        v28 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((134)) * 4)));
        if (!!((v10 != 0))) goto L42;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((134)) * 4))))));
        F_4B7160();
        M.WU32(v28, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L42: ;
        v10 = ((M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((136)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((136)) * 4))));
        v29 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((136)) * 4)));
        if (!!((v10 != 0))) goto L44;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((136)) * 4))))));
        F_4B7160();
        M.WU32(v29, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L44: ;
        v10 = ((M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((138)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((138)) * 4))));
        v30 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((138)) * 4)));
        if (!!((v10 != 0))) goto L46;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((138)) * 4))))));
        F_4B7160();
        M.WU32(v30, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L46: ;
        v10 = ((M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((140)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((140)) * 4))));
        v31 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((140)) * 4)));
        if (!!((v10 != 0))) goto L48;
        M.WI32(0x17C6DC4U, ((int)(((uint)(M.U32(0x17C6BF4U) + (uint)(((140)) * 4))))));
        F_4B7160();
        M.WU32(v31, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L48: ;
        v32 = M.U32(((uint)(M.U32(0x17C6BF4U) + (uint)(((142)) * 4))));
        v33 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((142)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(M.U32(0x17C6BF4U) + (uint)(((142)) * 4))));
        if (!(v32 != 0)) goto L50;
        if (!(v32 == ((uint)((-1))))) goto L52;
        v34 = M.U32(0x1EABC3CU);
        M.WU32(v33, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v34)));
        Nat.Load_XStringData(v33);
        goto L53;
        L52: ;
        M.WU32(v33, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v32 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v32 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L53: ;
        L50: ;
        v35 = ((uint)(M.U32(0x17C6BF4U) + (uint)(((143)) * 4)));
        v36 = (31);
        L54: ;
        v10 = ((M.U32(v35) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6CDCU, v35);
        if (!!((v10 != 0))) goto L57;
        M.WI32(0x17C6DC4U, ((int)(v35)));
        F_4B7160();
        M.WU32(v35, ((uint)(((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))))));
        L57: ;
        Rt::PreAdd(v35, 4);
        Rt::PreAdd(v36, -1);
        L55: ;
        if ((v36 != 0)) goto L54;
        L56: ;
        M.WU32(0x17C6CDCU, ((uint)(v35 - (uint)(((31)) * 4))));
        return F_4CEE70();
        return {};
    }

    // sub_4C54A0  int()
    int F_4C54A0()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6A38U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6A38U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6A38U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6A38U))), v2);
            M.WU32(0x17C6BF4U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4C4EC0();
            v4 = ((uint)(M.I32(0x17C6A38U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6A38U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((41))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6A38U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4C58A0  int()
    int F_4C58A0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        int v5 = 0;
        int v6 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6D6CU);
        M.WU32(M.U32(0x17C6D6CU), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6D6CU), ((uint)((16))));
        v0 = M.U32(0x17C6D6CU);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((16)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6D6CU);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((3)) * 4)))) != 0)) goto L7;
        v3 = M.U32(0x1EABC3CU);
        v4 = ((int)(M.U32(((uint)(v0 + (uint)(((1)) * 4))))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v3)));
        v5 = ((v4 != 0) ? v4 : ((int)(((uint)(M.U32(((uint)(v0 + (uint)(((2)) * 4)))) + (uint)(((1)) * 1))))));
        if (!(v5 != 0)) goto L9;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L11;
        Nat.Memset(v3, ((uint)((0))), ((uint)(v5)));
        M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)((v5) * 1))));
        return F_4CEE70();
        L11: ;
        Nat.DB_LoadXFileData(v3, ((uint)(v5)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v5) * 1))));
        L9: ;
        L7: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C59B0  int()
    int F_4C59B0()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x1746878U))))));
            if (!(M.U32(((uint)(M.I32(0x1746878U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x1746878U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x1746878U))), v2);
            M.WU32(0x17C6D6CU, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4C58A0();
            v4 = ((uint)(M.I32(0x1746878U)));
            v5 = ((uint)(M.I32(0x1746878U)));
            if (!(M.U32(M.U32(((uint)(M.I32(0x1746878U))))) != 0)) goto L7;
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x1746878U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v5, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((36))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            v4 = ((uint)(M.I32(0x1746878U)));
            L7: ;
            if (!(v3 != 0)) goto L9;
            M.WU32(v3, M.U32(v4));
            L9: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4C5AF0  void __usercall(int result@<eax>)
    void F_4C5AF0(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        v1 = result;
        v2 = ((int)(((uint)((8))) * ((uint)(result))));
        if (!(((int)(((uint)((8))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6CF0U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6CF0U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6CF0U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = M.U32(v3);
        M.WU32(0x17C6CF0U, v3);
        M.WU32(0x17C6EECU, v3);
        if (!(v4 != 0)) goto L10;
        if (!(v4 == ((uint)((-1))))) goto L12;
        v5 = M.U32(0x1EABC3CU);
        M.WU32(v3, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v5)));
        Nat.Load_XStringData(v3);
        goto L13;
        L12: ;
        M.WU32(v3, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v4 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v4 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L13: ;
        L10: ;
        v3 = ((uint)(v3 + (uint)(((2)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6CF0U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4C5B90  int()
    int F_4C5B90()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6D58U);
        M.WU32(M.U32(0x17C6D58U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6D58U), ((uint)((16))));
        v0 = M.U32(0x17C6D58U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((16)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6D58U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((3)) * 4)))) != 0)) goto L7;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), v3);
        M.WU32(0x17C6CF0U, v3);
        F_4C5AF0(((int)(M.U32(((uint)(v0 + (uint)(((1)) * 4)))) * M.U32(((uint)(v0 + (uint)(((2)) * 4)))))));
        L7: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C5C50  int()
    int F_4C5C50()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6B18U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6B18U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6B18U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6B18U))), v2);
            M.WU32(0x17C6D58U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4C5B90();
            v4 = ((uint)(M.I32(0x17C6B18U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6B18U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((37))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6B18U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4C5D90  void __usercall(int result@<eax>)
    void F_4C5D90(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        v1 = result;
        v2 = ((int)(((uint)((8))) * ((uint)(result))));
        if (!(((int)(((uint)((8))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6B3CU), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6B3CU), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6B3CU);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = M.U32(v3);
        M.WU32(0x17C6B3CU, v3);
        M.WU32(0x17C6EECU, v3);
        if (!(v4 != 0)) goto L10;
        if (!(v4 == ((uint)((-1))))) goto L12;
        v5 = M.U32(0x1EABC3CU);
        M.WU32(v3, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v5)));
        Nat.Load_XStringData(v3);
        goto L13;
        L12: ;
        M.WU32(v3, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v4 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v4 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L13: ;
        L10: ;
        v3 = ((uint)(v3 + (uint)(((2)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6B3CU, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4C5E30  void __usercall(int result@<eax>)
    void F_4C5E30(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        byte v4 = 0;
        uint v5 = 0;
        v1 = result;
        v2 = ((int)(((uint)((12))) * ((uint)(result))));
        if (!(((int)(((uint)((12))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C69B8U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C69B8U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C69B8U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = ((M.U32(((uint)(v3 + (uint)(((2)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C69B8U, v3);
        if (!!((v4 != 0))) goto L10;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v5);
        M.WU32(((uint)(v3 + (uint)(((2)) * 4))), v5);
        M.WU32(0x17C6B3CU, v5);
        F_4C5D90(((int)(M.U32(v3))));
        L10: ;
        v3 = ((uint)(v3 + (uint)(((3)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C69B8U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4C5EB0  void __usercall(int result@<eax>)
    void F_4C5EB0(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        v1 = result;
        v2 = ((int)(((uint)((24))) * ((uint)(result))));
        if (!(((int)(((uint)((24))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6A30U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A30U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6A30U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = M.U32(v3);
        M.WU32(0x17C6A30U, v3);
        M.WU32(0x17C6EECU, v3);
        if (!(v4 != 0)) goto L10;
        if (!(v4 == ((uint)((-1))))) goto L12;
        v5 = M.U32(0x1EABC3CU);
        M.WU32(v3, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v5)));
        Nat.Load_XStringData(v3);
        goto L13;
        L12: ;
        M.WU32(v3, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v4 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v4 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L13: ;
        L10: ;
        v3 = ((uint)(v3 + (uint)(((6)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6A30U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4C5F50  void __usercall(int result@<eax>)
    void F_4C5F50(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        byte v4 = 0;
        uint v5 = 0;
        v1 = result;
        v2 = ((int)(((uint)((20))) * ((uint)(result))));
        if (!(((int)(((uint)((20))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6C98U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6C98U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6C98U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = ((M.U32(((uint)(v3 + (uint)(((1)) * 4)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6C98U, v3);
        if (!!((v4 != 0))) goto L10;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v5);
        M.WU32(((uint)(v3 + (uint)(((1)) * 4))), v5);
        M.WU32(0x17C6A30U, v5);
        F_4C5EB0(((int)(M.U32(v3))));
        L10: ;
        v3 = ((uint)(v3 + (uint)(((5)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6C98U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4C5FD0  void __thiscall(void *this)
    void F_4C5FD0(uint l_this)
    {
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        int v5 = 0;
        int v6 = 0;
        uint v7 = 0;
        int v8 = 0;
        byte v9 = 0;
        int v10 = 0;
        v1 = M.U32(0x17C6CC8U);
        if (!(M.U32(((uint)(M.U32(0x17C6CC8U) + (uint)(((3)) * 4)))) != 0)) goto L1;
        v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v2);
        M.WU32(((uint)(M.U32(0x17C6CC8U) + (uint)(((3)) * 4))), v2);
        M.WU32(0x17C69B8U, v2);
        F_4C5E30(M.I32(((uint)(v1 + (uint)(((2)) * 4)))));
        v1 = M.U32(0x17C6CC8U);
        L1: ;
        if (!(M.I32(((uint)(v1 + (uint)(((5)) * 4)))) != 0)) goto L3;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WI32(((uint)(v1 + (uint)(((5)) * 4))), ((int)(v3)));
        M.WU32(0x17C6C98U, v3);
        F_4C5F50(M.I32(((uint)(v1 + (uint)(((4)) * 4)))));
        v1 = M.U32(0x17C6CC8U);
        L3: ;
        v4 = M.U32(0x1EABC3CU);
        if (!(M.I32(((uint)(v1 + (uint)(((7)) * 4)))) != 0)) goto L5;
        v4 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v5 = ((int)(((uint)((16))) * ((uint)(M.I32(((uint)(v1 + (uint)(((6)) * 4))))))));
        M.WU32(0x1EABC3CU, v4);
        M.WI32(((uint)(v1 + (uint)(((7)) * 4))), ((int)(v4)));
        M.WI32(0x17C6D20U, ((int)(v4)));
        if (!(v5 != 0)) goto L7;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L9;
        Nat.Memset(v4, ((uint)((0))), ((uint)(v5)));
        goto L10;
        L9: ;
        Nat.DB_LoadXFileData(v4, ((uint)(v5)));
        v1 = M.U32(0x17C6CC8U);
        v4 = M.U32(0x1EABC3CU);
        L10: ;
        v4 = ((uint)(v4 + (uint)((v5) * 1)));
        M.WU32(0x1EABC3CU, v4);
        L7: ;
        L5: ;
        if (!(M.I32(((uint)(v1 + (uint)(((9)) * 4)))) != 0)) goto L11;
        v4 = ((uint)(((uint)(v4 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v6 = ((int)(((uint)((16))) * ((uint)(M.I32(((uint)(v1 + (uint)(((8)) * 4))))))));
        M.WU32(0x1EABC3CU, v4);
        M.WI32(((uint)(v1 + (uint)(((9)) * 4))), ((int)(v4)));
        M.WI32(0x17C6F20U, ((int)(v4)));
        if (!(v6 != 0)) goto L13;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L15;
        Nat.Memset(v4, ((uint)((0))), ((uint)(v6)));
        goto L16;
        L15: ;
        Nat.DB_LoadXFileData(v4, ((uint)(v6)));
        v1 = M.U32(0x17C6CC8U);
        v4 = M.U32(0x1EABC3CU);
        L16: ;
        v4 = ((uint)(v4 + (uint)((v6) * 1)));
        M.WU32(0x1EABC3CU, v4);
        L13: ;
        L11: ;
        if (!(M.I32(((uint)(v1 + (uint)(((14)) * 4)))) != 0)) goto L17;
        v7 = ((uint)(((uint)(v4 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WI32(((uint)(v1 + (uint)(((14)) * 4))), ((int)(v7)));
        v8 = ((int)(((uint)((2))) * ((uint)(M.I32(((uint)(v1 + (uint)(((13)) * 4))))))));
        v9 = ((((uint)(((int)(((uint)((2))) * ((uint)(v8)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v10 = ((int)(((uint)((2))) * ((uint)(v8))));
        M.WU32(0x1EABC3CU, v7);
        M.WU32(0x17C693CU, v7);
        if (!!((v9 != 0))) goto L19;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L21;
        Nat.Memset(v7, ((uint)((0))), ((uint)(v10)));
        M.WU32(0x1EABC3CU, ((uint)(v7 + (uint)((v10) * 1))));
        goto L22;
        L21: ;
        Nat.DB_LoadXFileData(v7, ((uint)(v10)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v10) * 1))));
        L22: ;
        L19: ;
        L17: ;
    }

    // sub_4C6150  void __cdecl(int a1)
    void F_4C6150(int a1)
    {
        uint v1 = 0;
        int v2 = 0;
        int v3 = 0;
        uint v4 = 0;
        v2 = a1;
        v3 = ((int)(((uint)((60))) * ((uint)(a1))));
        if (!(((int)(((uint)((60))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6CC8U), ((uint)((0))), ((uint)(((int)(((uint)((60))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6CC8U), ((uint)(((int)(((uint)((60))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v3) * 1))));
        L1: ;
        v4 = M.U32(0x17C6CC8U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6CC8U, v4);
        F_4C5FD0(v1);
        v4 = ((uint)(v4 + (uint)(((60)) * 1)));
        Rt::PreAdd(v2, -1);
        L8: ;
        if ((v2 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6CC8U, ((uint)(v4 + (uint)((((int)(0 - v3))) * 1))));
    }

    // sub_4C61C0  int()
    int F_4C61C0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6DF8U);
        M.WU32(M.U32(0x17C6DF8U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6DF8U), ((uint)((12))));
        v0 = M.U32(0x17C6DF8U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((12)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6DF8U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((2)) * 4)))) != 0)) goto L7;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), v3);
        M.WU32(0x17C6CC8U, v3);
        F_4C6150(((int)(M.U32(((uint)(v0 + (uint)(((1)) * 4)))))));
        L7: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C6280  int()
    int F_4C6280()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6ED8U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6ED8U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6ED8U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6ED8U))), v2);
            M.WU32(0x17C6DF8U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4C61C0();
            v4 = ((uint)(M.I32(0x17C6ED8U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6ED8U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((39))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6ED8U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4C63C0  char **()
    uint F_4C63C0()
    {
        uint v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint result = 0;
        uint v5 = 0;
        v0 = M.U32(0x17C6CA8U);
        v1 = ((int)(M.U32(M.U32(0x17C6CA8U))));
        M.WU32(0x17C6EECU, M.U32(0x17C6CA8U));
        if (!(v1 != 0)) goto L1;
        if (!(((uint)(v1)) == ((uint)((-1))))) goto L3;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(M.U32(0x17C6CA8U), M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6CA8U);
        goto L4;
        L3: ;
        M.WU32(M.U32(0x17C6CA8U), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L4: ;
        L1: ;
        v3 = M.U32(((uint)(v0 + (uint)(((4)) * 4))));
        result = ((uint)(v0 + (uint)(((4)) * 4)));
        M.WU32(0x17C6EECU, result);
        if (!(v3 != 0)) goto L5;
        if (!(v3 == ((uint)((-1))))) goto L7;
        v5 = M.U32(0x1EABC3CU);
        M.WU32(result, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v5)));
        return ((uint)(Nat.Load_XStringData(result)));
        goto L8;
        L7: ;
        M.WU32(result, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v3 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v3 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L8: ;
        L5: ;
        return result;
        return {};
    }

    // sub_4C6460  void __cdecl(int a1)
    void F_4C6460(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((32))) * ((uint)(a1))));
        if (!(((int)(((uint)((32))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6CA8U), ((uint)((0))), ((uint)(((int)(((uint)((32))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6CA8U), ((uint)(((int)(((uint)((32))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6CA8U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6CA8U, v3);
        F_4C63C0();
        v3 = ((uint)(v3 + (uint)(((8)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6CA8U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4C64D0  int()
    int F_4C64D0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6EB8U);
        M.WU32(M.U32(0x17C6EB8U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((4)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((5)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6EB8U), ((uint)((24))));
        v0 = M.U32(0x17C6EB8U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((24)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6EB8U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        if (!(M.U32(((uint)(v0 + (uint)(((5)) * 4)))) != 0)) goto L7;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(v0 + (uint)(((5)) * 4))), v3);
        M.WU32(0x17C6CA8U, v3);
        F_4C6460(((int)(M.U32(((uint)(v0 + (uint)(((2)) * 4)))))));
        L7: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C65A0  int()
    int F_4C65A0()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6DD8U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6DD8U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6DD8U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6DD8U))), v2);
            M.WU32(0x17C6EB8U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4C64D0();
            v4 = ((uint)(M.I32(0x17C6DD8U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6DD8U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((38))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6DD8U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // sub_4C66E0  void __cdecl(int a1)
    void F_4C66E0(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((76))) * ((uint)(a1))));
        if (!(((int)(((uint)((76))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6DA0U), ((uint)((0))), ((uint)(((int)(((uint)((76))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6DA0U), ((uint)(((int)(((uint)((76))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6DA0U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6DA0U, v3);
        M.WI32(0x17C69D4U, ((int)(((uint)(v3 + (uint)(((52)) * 1))))));
        F_4BBEF0();
        v3 = ((uint)(v3 + (uint)(((76)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6DA0U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4C68E0  void __usercall(unsigned int result@<eax>)
    void F_4C68E0(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((56))) * result))));
        if (!(((uint)(((uint)((56))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6A18U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A18U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4C6930  void __cdecl(signed int a1)
    void F_4C6930(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        int v5 = 0;
        int v6 = 0;
        byte v7 = 0;
        int v8 = 0;
        int v9 = 0;
        int v10 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((44))) * ((uint)(a1))));
        v9 = ((int)(((uint)((44))) * ((uint)(a1))));
        if (!(((int)(((uint)((44))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6C4CU), ((uint)((0))), ((uint)(((int)(((uint)((44))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6C4CU), ((uint)(((int)(((uint)((44))) * ((uint)(a1)))))));
        L4: ;
        v1 = a1;
        v3 = ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        goto L2;
        L1: ;
        v3 = M.U32(0x1EABC3CU);
        L2: ;
        v4 = M.U32(0x17C6C4CU);
        if (!(v1 > (0))) goto L5;
        v10 = v1;
        L7: ;
        v5 = ((int)(M.U32(((uint)(v4 + (uint)(((9)) * 4))))));
        M.WU32(0x17C6C4CU, v4);
        if (!(v5 != 0)) goto L10;
        if (!(((uint)(v5)) == ((uint)((-1))))) goto L12;
        v6 = ((int)(M.U16(((uint)(v4 + (uint)(((17)) * 2))))));
        v3 = ((uint)(((uint)(v3 + (uint)(((1)) * 1))) & 0xFFFFFFFEU));
        v7 = ((((uint)(((int)(((uint)((2))) * ((uint)(v6)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v8 = ((int)(((uint)((2))) * ((uint)(v6))));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(v4 + (uint)(((9)) * 4))), v3);
        M.WI32(0x17C6D98U, ((int)(v3)));
        if (!!((v7 != 0))) goto L14;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L16;
        Nat.Memset(v3, ((uint)((0))), ((uint)(v8)));
        goto L17;
        L16: ;
        Nat.DB_LoadXFileData(v3, ((uint)(v8)));
        v3 = M.U32(0x1EABC3CU);
        L17: ;
        v3 = ((uint)(v3 + (uint)((v8) * 1)));
        M.WU32(0x1EABC3CU, v3);
        L14: ;
        v2 = v9;
        goto L13;
        L12: ;
        M.WU32(((uint)(v4 + (uint)(((9)) * 4))), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v5)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v5)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L13: ;
        L10: ;
        v4 = ((uint)(v4 + (uint)(((11)) * 4)));
        Rt::PreAdd(v10, -1);
        L8: ;
        if ((v10 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6C4CU, ((uint)(v4 - (uint)((v2) * 1))));
    }

    // sub_4C6A20  void __usercall(int result@<eax>)
    void F_4C6A20(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        byte v4 = 0;
        uint v5 = 0;
        v1 = result;
        v2 = ((int)(((uint)((4))) * ((uint)(result))));
        if (!(((int)(((uint)((4))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6964U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6964U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6964U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = ((M.U32(v3) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6964U, v3);
        if (!!((v4 != 0))) goto L10;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(v3, v5);
        M.WU32(0x1EABC3CU, v5);
        M.WU32(0x17C6C4CU, v5);
        F_4C6930(((int)(M.U32(((uint)(M.U32(((uint)(M.U32(0x17C6940U) + (uint)(((17)) * 4)))) + (uint)((((int)(((uint)((4))) * ((uint)(((int)(((int)((int)(v3 - M.U32(((uint)(M.U32(0x17C6940U) + (uint)(((18)) * 4))))) / 1)) >> (2)))))))) * 1)))))));
        L10: ;
        Rt::PreAdd(v3, 4);
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6964U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4C6AC0  void()
    void F_4C6AC0()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        v0 = M.U32(0x17C69F0U);
        if (!(M.U32(((uint)(M.U32(0x17C69F0U) + (uint)(((7)) * 4)))) != 0)) goto L1;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(M.U32(0x17C69F0U) + (uint)(((7)) * 4))), v1);
        v4 = ((int)(M.U32(((uint)(v0 + (uint)(((6)) * 4))))));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(0x17C6948U, v1);
        F_4C6C50(v4);
        v0 = M.U32(0x17C69F0U);
        L1: ;
        if (!(M.U32(((uint)(v0 + (uint)(((9)) * 4)))) != 0)) goto L3;
        v2 = ((int)(M.U8(((uint)(v0 + (uint)(((32)) * 1))))));
        v3 = M.U32(0x1EABC3CU);
        M.WU32(((uint)(v0 + (uint)(((9)) * 4))), M.U32(0x1EABC3CU));
        M.WU32(0x17C6AFCU, v3);
        if (!(v2 != 0)) goto L5;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L7;
        Nat.Memset(v3, ((uint)((0))), ((uint)(v2)));
        M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)((v2) * 1))));
        goto L8;
        L7: ;
        Nat.DB_LoadXFileData(v3, ((uint)(v2)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L8: ;
        L5: ;
        L3: ;
    }

    // sub_4C6B60  void __cdecl(int a1)
    void F_4C6B60(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((40))) * ((uint)(a1))));
        if (!(((int)(((uint)((40))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C69F0U), ((uint)((0))), ((uint)(((int)(((uint)((40))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C69F0U), ((uint)(((int)(((uint)((40))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C69F0U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C69F0U, v3);
        F_4C6AC0();
        v3 = ((uint)(v3 + (uint)(((40)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C69F0U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4C6BD0  void()
    void F_4C6BD0()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        int v3 = 0;
        v0 = M.U32(0x17C6948U);
        if (!(M.U32(((uint)(M.U32(0x17C6948U) + (uint)(((7)) * 4)))) != 0)) goto L1;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(M.U32(0x17C6948U) + (uint)(((7)) * 4))), v1);
        v2 = ((int)(M.U8(((uint)(v0 + (uint)(((34)) * 1))))));
        v3 = ((int)(((uint)((12))) * ((uint)(v2))));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(0x17C6BBCU, v1);
        if (!(((int)(((uint)((12))) * ((uint)(v2)))) != 0)) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(v1, ((uint)((0))), ((uint)(((int)(((uint)((12))) * ((uint)(v2)))))));
        M.WU32(0x1EABC3CU, ((uint)(v1 + (uint)((v3) * 1))));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v1, ((uint)(((int)(((uint)((12))) * ((uint)(v2)))))));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v3) * 1))));
        L6: ;
        L3: ;
        L1: ;
    }

    // sub_4C6C50  void __cdecl(int a1)
    void F_4C6C50(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((60))) * ((uint)(a1))));
        if (!(((int)(((uint)((60))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6948U), ((uint)((0))), ((uint)(((int)(((uint)((60))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6948U), ((uint)(((int)(((uint)((60))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6948U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6948U, v3);
        F_4C6BD0();
        v3 = ((uint)(v3 + (uint)(((60)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6948U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4C6CC0  void __usercall(int result@<eax>)
    void F_4C6CC0(int result)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = result;
        v2 = ((int)(((uint)((8))) * ((uint)(result))));
        if (!(((int)(((uint)((8))) * ((uint)(result)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6D40U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6D40U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6D40U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6D40U, v3);
        M.WU32(0x17C698CU, v3);
        F_4BA420();
        v3 = ((uint)(v3 + (uint)(((8)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6D40U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4C6D30  int()
    int F_4C6D30()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        int v3 = 0;
        int v4 = 0;
        int v5 = 0;
        int v6 = 0;
        v0 = M.I32(0x17C6D7CU);
        if (!(M.U32(((uint)(M.I32(0x17C6D7CU)))) != 0)) goto L1;
        v1 = M.I32(0x17C69B0U);
        v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(M.I32(0x17C6D7CU))), v2);
        v3 = ((int)(((uint)((44))) * M.U32(((uint)(((int)(((uint)(v1)) + ((uint)((40))))))))));
        M.WU32(0x1EABC3CU, v2);
        M.WI32(0x17C6F5CU, ((int)(v2)));
        if (!(v3 != 0)) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(v2, ((uint)((0))), ((uint)(v3)));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v2, ((uint)(v3)));
        v0 = M.I32(0x17C6D7CU);
        v2 = M.U32(0x1EABC3CU);
        L6: ;
        M.WU32(0x1EABC3CU, ((uint)(v2 + (uint)((v3) * 1))));
        L3: ;
        L1: ;
        v6 = ((int)(((uint)((44))) * M.U32(((uint)(((int)(((uint)(M.I32(0x17C69B0U))) + ((uint)((40))))))))));
        v5 = ((int)(M.U32(((uint)(((int)(((uint)(M.I32(0x17C69B0U))) + ((uint)((44))))))))));
        M.WI32(0x17C6F28U, ((int)(((uint)(v0)) + ((uint)((4))))));
        return ((int)(Nat.Stub(0xA916D0U)));
        return {};
    }

    // sub_4C6DD0  int()
    int F_4C6DD0()
    {
        int v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        int v3 = 0;
        int v4 = 0;
        int v5 = 0;
        int v6 = 0;
        v0 = M.I32(0x17C6F34U);
        if (!(M.U32(((uint)(M.I32(0x17C6F34U)))) != 0)) goto L1;
        v1 = M.U32(0x1EABC3CU);
        v2 = M.I32(0x17C69B0U);
        M.WU32(((uint)(M.I32(0x17C6F34U))), M.U32(0x1EABC3CU));
        v3 = ((int)(M.U32(((uint)(((int)(((uint)(v2)) + ((uint)((52))))))))));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(0x17C6AFCU, v1);
        if (!(v3 != 0)) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(v1, ((uint)((0))), ((uint)(v3)));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v1, ((uint)(v3)));
        v0 = M.I32(0x17C6F34U);
        v1 = M.U32(0x1EABC3CU);
        L6: ;
        M.WU32(0x1EABC3CU, ((uint)(v1 + (uint)((v3) * 1))));
        L3: ;
        L1: ;
        v6 = ((int)(M.U32(((uint)(((int)(((uint)(M.I32(0x17C69B0U))) + ((uint)((52))))))))));
        v5 = ((int)(M.U32(((uint)(((int)(((uint)(M.I32(0x17C69B0U))) + ((uint)((56))))))))));
        M.WI32(0x17C6F28U, ((int)(((uint)(v0)) + ((uint)((4))))));
        return ((int)(Nat.Stub(0xA916D0U)));
        return {};
    }

    // sub_4C6E60  void()
    void F_4C6E60()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        int v3 = 0;
        int v4 = 0;
        byte v5 = 0;
        int v6 = 0;
        uint v7 = 0;
        int v8 = 0;
        v0 = M.U32(0x1EABC3CU);
        v1 = ((uint)(M.I32(0x17C6C6CU)));
        if (!(M.U32(((uint)(((int)(((uint)(M.I32(0x17C6C6CU))) + ((uint)((28)))))))) != 0)) goto L1;
        v2 = ((int)(M.U32(((uint)(((int)(((uint)(M.I32(0x17C6C6CU))) + ((uint)((20))))))))));
        v0 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((1))))) & 0xFFFFFFFEU));
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C6C6CU))) + ((uint)((28))))))), v0);
        v3 = ((int)(((uint)(((int)(((uint)((2))) * ((uint)(((int)(((uint)(M.U16(((uint)(v1 + (uint)((((int)(((uint)(v2)) + ((uint)((7)))))) * 2)))))) - ((uint)(M.U16(((uint)(v1 + (uint)((((int)(((uint)(v2)) + ((uint)((4)))))) * 2)))))))))))))) + ((uint)((2)))));
        M.WU32(0x1EABC3CU, v0);
        M.WU32(0x17C69ACU, v0);
        if (!(v3 != 0)) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(v0, ((uint)((0))), ((uint)(v3)));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v0, ((uint)(v3)));
        v1 = ((uint)(M.I32(0x17C6C6CU)));
        v0 = M.U32(0x1EABC3CU);
        L6: ;
        v0 = ((uint)(v0 + (uint)((v3) * 1)));
        M.WU32(0x1EABC3CU, v0);
        L3: ;
        L1: ;
        if (!(M.U32(((uint)(v1 + (uint)(((9)) * 4)))) != 0)) goto L7;
        v4 = ((int)(M.U32(((uint)(v1 + (uint)(((8)) * 4))))));
        M.WU32(0x1EABC3CU, v0);
        M.WU32(((uint)(v1 + (uint)(((9)) * 4))), v0);
        M.WU32(0x17C6AFCU, v0);
        if (!(v4 != 0)) goto L9;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L11;
        Nat.Memset(v0, ((uint)((0))), ((uint)(v4)));
        goto L12;
        L11: ;
        Nat.DB_LoadXFileData(v0, ((uint)(v4)));
        v1 = ((uint)(M.I32(0x17C6C6CU)));
        v0 = M.U32(0x1EABC3CU);
        L12: ;
        v0 = ((uint)(v0 + (uint)((v4) * 1)));
        M.WU32(0x1EABC3CU, v0);
        L9: ;
        L7: ;
        if (!(M.U32(((uint)(v1 + (uint)(((11)) * 4)))) != 0)) goto L13;
        v0 = ((uint)(((uint)(v0 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v5 = ((((uint)(((int)(((uint)((4))) * M.U32(((uint)(v1 + (uint)(((10)) * 4)))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v6 = ((int)(((uint)((4))) * M.U32(((uint)(v1 + (uint)(((10)) * 4))))));
        M.WU32(0x1EABC3CU, v0);
        M.WU32(((uint)(v1 + (uint)(((11)) * 4))), v0);
        M.WI32(0x17C6AD4U, ((int)(v0)));
        if (!!((v5 != 0))) goto L15;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L17;
        Nat.Memset(v0, ((uint)((0))), ((uint)(v6)));
        goto L18;
        L17: ;
        Nat.DB_LoadXFileData(v0, ((uint)(v6)));
        v1 = ((uint)(M.I32(0x17C6C6CU)));
        v0 = M.U32(0x1EABC3CU);
        L18: ;
        v0 = ((uint)(v0 + (uint)((v6) * 1)));
        M.WU32(0x1EABC3CU, v0);
        L15: ;
        L13: ;
        if (!(M.U32(((uint)(v1 + (uint)(((13)) * 4)))) != 0)) goto L19;
        v7 = ((uint)(((uint)(v0 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WU32(((uint)(v1 + (uint)(((13)) * 4))), v7);
        v8 = ((int)(((uint)((168))) * M.U32(((uint)(v1 + (uint)(((12)) * 4))))));
        M.WU32(0x1EABC3CU, v7);
        M.WI32(0x17C6B2CU, ((int)(v7)));
        if (!(v8 != 0)) goto L21;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L23;
        Nat.Memset(v7, ((uint)((0))), ((uint)(v8)));
        M.WU32(0x1EABC3CU, ((uint)(v7 + (uint)((v8) * 1))));
        goto L24;
        L23: ;
        Nat.DB_LoadXFileData(v7, ((uint)(v8)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v8) * 1))));
        L24: ;
        L21: ;
        L19: ;
    }

    // sub_4C6FE0  void()
    void F_4C6FE0()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        byte v3 = 0;
        int v4 = 0;
        int v5 = 0;
        uint v6 = 0;
        int v7 = 0;
        v0 = M.U32(0x17C6BACU);
        v1 = M.U32(0x1EABC3CU);
        if (!(M.U32(((uint)(M.U32(0x17C6BACU) + (uint)(((1)) * 4)))) != 0)) goto L1;
        v2 = ((int)(M.U16(M.U32(0x17C6BACU))));
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((1))))) & 0xFFFFFFFEU));
        v3 = ((((uint)(((int)(((uint)((2))) * ((uint)(v2)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v4 = ((int)(((uint)((2))) * ((uint)(v2))));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(M.U32(0x17C6BACU) + (uint)(((1)) * 4))), v1);
        M.WU32(0x17C69ACU, v1);
        if (!!((v3 != 0))) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(v1, ((uint)((0))), ((uint)(v4)));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v1, ((uint)(v4)));
        v0 = M.U32(0x17C6BACU);
        v1 = M.U32(0x1EABC3CU);
        L6: ;
        v1 = ((uint)(v1 + (uint)((v4) * 1)));
        M.WU32(0x1EABC3CU, v1);
        L3: ;
        L1: ;
        if (!(M.U32(((uint)(v0 + (uint)(((2)) * 4)))) != 0)) goto L7;
        v5 = ((int)(M.U16(((uint)(v0 + (uint)(((1)) * 2))))));
        v6 = ((uint)(((uint)(v1 + (uint)(((1)) * 1))) & 0xFFFFFFFEU));
        v3 = ((((uint)(((int)(((uint)((2))) * ((uint)(v5)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v7 = ((int)(((uint)((2))) * ((uint)(v5))));
        M.WU32(0x1EABC3CU, v6);
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), v6);
        M.WU32(0x17C69ACU, v6);
        if (!!((v3 != 0))) goto L9;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L11;
        Nat.Memset(v6, ((uint)((0))), ((uint)(v7)));
        M.WU32(0x1EABC3CU, ((uint)(v6 + (uint)((v7) * 1))));
        goto L12;
        L11: ;
        Nat.DB_LoadXFileData(v6, ((uint)(v7)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v7) * 1))));
        L12: ;
        L9: ;
        L7: ;
    }

    // sub_4C70A0  void __cdecl(int a1)
    void F_4C70A0(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((12))) * ((uint)(a1))));
        if (!(((int)(((uint)((12))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6BACU), ((uint)((0))), ((uint)(((int)(((uint)((12))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6BACU), ((uint)(((int)(((uint)((12))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6BACU);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6BACU, v3);
        F_4C6FE0();
        v3 = ((uint)(v3 + (uint)(((12)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6BACU, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4C7110  void()
    void F_4C7110()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        int v3 = 0;
        v0 = M.U32(0x17C69D0U);
        if (!(M.U32(((uint)(M.U32(0x17C69D0U) + (uint)(((19)) * 4)))) != 0)) goto L1;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(M.U32(0x17C69D0U) + (uint)(((19)) * 4))), v1);
        v2 = ((int)(M.U32(((uint)(v0 + (uint)(((18)) * 4))))));
        v3 = ((int)(((uint)((20))) * ((uint)(v2))));
        M.WU32(0x1EABC3CU, v1);
        M.WI32(0x17C6E10U, ((int)(v1)));
        if (!(((int)(((uint)((20))) * ((uint)(v2)))) != 0)) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(v1, ((uint)((0))), ((uint)(((int)(((uint)((20))) * ((uint)(v2)))))));
        M.WU32(0x1EABC3CU, ((uint)(v1 + (uint)((v3) * 1))));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v1, ((uint)(((int)(((uint)((20))) * ((uint)(v2)))))));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v3) * 1))));
        L6: ;
        L3: ;
        L1: ;
    }

    // sub_4C7190  void __cdecl(int a1)
    void F_4C7190(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((80))) * ((uint)(a1))));
        if (!(((int)(((uint)((80))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C69D0U), ((uint)((0))), ((uint)(((int)(((uint)((80))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C69D0U), ((uint)(((int)(((uint)((80))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C69D0U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C69D0U, v3);
        F_4C7110();
        v3 = ((uint)(v3 + (uint)(((80)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C69D0U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4C7200  void __usercall(int a1@<eax>)
    void F_4C7200(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        byte v4 = 0;
        uint v5 = 0;
        int v6 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((8))) * ((uint)(a1))));
        if (!(((int)(((uint)((8))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6B94U), ((uint)((0))), ((uint)(v2)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6B94U), ((uint)(v2)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6B94U);
        if (!(v1 > (0))) goto L5;
        L7: ;
        v4 = ((((uint)(M.I32(((uint)(v3 + (uint)(((1)) * 4)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        M.WU32(0x17C6B94U, v3);
        if (!!((v4 != 0))) goto L10;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v6 = M.I32(v3);
        M.WU32(0x1EABC3CU, v5);
        M.WI32(((uint)(v3 + (uint)(((1)) * 4))), ((int)(v5)));
        M.WU32(0x17C69D0U, v5);
        F_4C7190(v6);
        L10: ;
        v3 = ((uint)(v3 + (uint)(((2)) * 4)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6B94U, ((uint)(v3 + (uint)((((int)(((uint)(((uint)(v2)) / 0xFFFFFFFCU))))) * 4))));
    }

    // sub_4C7290  int()
    int F_4C7290()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        byte v3 = 0;
        int v4 = 0;
        uint v5 = 0;
        uint v6 = 0;
        int v7 = 0;
        uint v8 = 0;
        int v9 = 0;
        uint v10 = 0;
        int v11 = 0;
        uint v12 = 0;
        int v13 = 0;
        uint v14 = 0;
        int v15 = 0;
        uint v16 = 0;
        int v17 = 0;
        uint v18 = 0;
        int v19 = 0;
        int v20 = 0;
        F_4CEE10((2));
        v0 = ((uint)(M.I32(0x17C6EF4U)));
        if (!(M.U32(((uint)(((int)(((uint)(M.I32(0x17C6EF4U))) + ((uint)((16)))))))) != 0)) goto L1;
        v1 = M.U32(0x17C6940U);
        v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C6EF4U))) + ((uint)((16))))))), v2);
        v3 = ((((uint)(((int)(((uint)((4))) * ((uint)(((int)(M.U32(v0) * M.U32(((uint)(v1 + (uint)(((13)) * 4)))))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v4 = ((int)(((uint)((4))) * ((uint)(((int)(M.U32(v0) * M.U32(((uint)(v1 + (uint)(((13)) * 4))))))))));
        M.WU32(0x1EABC3CU, v2);
        M.WI32(0x17C6C80U, ((int)(v2)));
        if (!!((v3 != 0))) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(v2, ((uint)((0))), ((uint)(v4)));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v2, ((uint)(v4)));
        v0 = ((uint)(M.I32(0x17C6EF4U)));
        v2 = M.U32(0x1EABC3CU);
        L6: ;
        M.WU32(0x1EABC3CU, ((uint)(v2 + (uint)((v4) * 1))));
        L3: ;
        L1: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v0 + (uint)(((5)) * 4)))) != 0)) goto L7;
        v5 = M.U32(0x17C6940U);
        v6 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(v0 + (uint)(((5)) * 4))), v6);
        v3 = ((((uint)(((int)(((uint)((4))) * ((uint)(((int)(M.U32(((uint)(v0 + (uint)(((1)) * 4)))) * M.U32(((uint)(v5 + (uint)(((13)) * 4)))))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v7 = ((int)(((uint)((4))) * ((uint)(((int)(M.U32(((uint)(v0 + (uint)(((1)) * 4)))) * M.U32(((uint)(v5 + (uint)(((13)) * 4))))))))));
        M.WU32(0x1EABC3CU, v6);
        M.WI32(0x17C6C80U, ((int)(v6)));
        if (!!((v3 != 0))) goto L9;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L11;
        Nat.Memset(v6, ((uint)((0))), ((uint)(v7)));
        goto L12;
        L11: ;
        Nat.DB_LoadXFileData(v6, ((uint)(v7)));
        v0 = ((uint)(M.I32(0x17C6EF4U)));
        v6 = M.U32(0x1EABC3CU);
        L12: ;
        M.WU32(0x1EABC3CU, ((uint)(v6 + (uint)((v7) * 1))));
        L9: ;
        L7: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v0 + (uint)(((6)) * 4)))) != 0)) goto L13;
        v8 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((15))))) & 0xFFFFFFF0U));
        v9 = ((int)(((uint)((32))) * M.U32(v0)));
        M.WU32(0x1EABC3CU, v8);
        M.WU32(((uint)(v0 + (uint)(((6)) * 4))), v8);
        M.WI32(0x17C6E44U, ((int)(v8)));
        if (!(v9 != 0)) goto L15;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L17;
        Nat.Memset(v8, ((uint)((0))), ((uint)(v9)));
        goto L18;
        L17: ;
        Nat.DB_LoadXFileData(v8, ((uint)(v9)));
        v0 = ((uint)(M.I32(0x17C6EF4U)));
        v8 = M.U32(0x1EABC3CU);
        L18: ;
        M.WU32(0x1EABC3CU, ((uint)(v8 + (uint)((v9) * 1))));
        L15: ;
        L13: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v0 + (uint)(((9)) * 4)))) != 0)) goto L19;
        v10 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((15))))) & 0xFFFFFFF0U));
        v11 = ((int)(((uint)((32))) * M.U32(((uint)(v0 + (uint)(((1)) * 4))))));
        M.WU32(0x1EABC3CU, v10);
        M.WU32(((uint)(v0 + (uint)(((9)) * 4))), v10);
        M.WI32(0x17C6E44U, ((int)(v10)));
        if (!(v11 != 0)) goto L21;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L23;
        Nat.Memset(v10, ((uint)((0))), ((uint)(v11)));
        goto L24;
        L23: ;
        Nat.DB_LoadXFileData(v10, ((uint)(v11)));
        v0 = ((uint)(M.I32(0x17C6EF4U)));
        v10 = M.U32(0x1EABC3CU);
        L24: ;
        M.WU32(0x1EABC3CU, ((uint)(v10 + (uint)((v11) * 1))));
        L21: ;
        L19: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v0 + (uint)(((7)) * 4)))) != 0)) goto L25;
        v12 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((15))))) & 0xFFFFFFF0U));
        v13 = ((int)(((uint)((32))) * M.U32(v0)));
        M.WU32(0x1EABC3CU, v12);
        M.WU32(((uint)(v0 + (uint)(((7)) * 4))), v12);
        M.WI32(0x17C6E44U, ((int)(v12)));
        if (!(v13 != 0)) goto L27;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L29;
        Nat.Memset(v12, ((uint)((0))), ((uint)(v13)));
        goto L30;
        L29: ;
        Nat.DB_LoadXFileData(v12, ((uint)(v13)));
        v0 = ((uint)(M.I32(0x17C6EF4U)));
        v12 = M.U32(0x1EABC3CU);
        L30: ;
        M.WU32(0x1EABC3CU, ((uint)(v12 + (uint)((v13) * 1))));
        L27: ;
        L25: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v0 + (uint)(((10)) * 4)))) != 0)) goto L31;
        v14 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((15))))) & 0xFFFFFFF0U));
        v15 = ((int)(((uint)((32))) * M.U32(((uint)(v0 + (uint)(((1)) * 4))))));
        M.WU32(0x1EABC3CU, v14);
        M.WU32(((uint)(v0 + (uint)(((10)) * 4))), v14);
        M.WI32(0x17C6E44U, ((int)(v14)));
        if (!(v15 != 0)) goto L33;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L35;
        Nat.Memset(v14, ((uint)((0))), ((uint)(v15)));
        goto L36;
        L35: ;
        Nat.DB_LoadXFileData(v14, ((uint)(v15)));
        v0 = ((uint)(M.I32(0x17C6EF4U)));
        v14 = M.U32(0x1EABC3CU);
        L36: ;
        M.WU32(0x1EABC3CU, ((uint)(v14 + (uint)((v15) * 1))));
        L33: ;
        L31: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v0 + (uint)(((8)) * 4)))) != 0)) goto L37;
        v16 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((15))))) & 0xFFFFFFF0U));
        v17 = ((int)(((uint)((32))) * M.U32(v0)));
        M.WU32(0x1EABC3CU, v16);
        M.WU32(((uint)(v0 + (uint)(((8)) * 4))), v16);
        M.WI32(0x17C6E44U, ((int)(v16)));
        if (!(v17 != 0)) goto L39;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L41;
        Nat.Memset(v16, ((uint)((0))), ((uint)(v17)));
        goto L42;
        L41: ;
        Nat.DB_LoadXFileData(v16, ((uint)(v17)));
        v0 = ((uint)(M.I32(0x17C6EF4U)));
        v16 = M.U32(0x1EABC3CU);
        L42: ;
        M.WU32(0x1EABC3CU, ((uint)(v16 + (uint)((v17) * 1))));
        L39: ;
        L37: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v0 + (uint)(((11)) * 4)))) != 0)) goto L43;
        v18 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((15))))) & 0xFFFFFFF0U));
        M.WU32(((uint)(v0 + (uint)(((11)) * 4))), v18);
        v19 = ((int)(((uint)((32))) * M.U32(((uint)(v0 + (uint)(((1)) * 4))))));
        M.WU32(0x1EABC3CU, v18);
        M.WI32(0x17C6E44U, ((int)(v18)));
        if (!(v19 != 0)) goto L45;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L47;
        Nat.Memset(v18, ((uint)((0))), ((uint)(v19)));
        M.WU32(0x1EABC3CU, ((uint)(v18 + (uint)((v19) * 1))));
        goto L48;
        L47: ;
        Nat.DB_LoadXFileData(v18, ((uint)(v19)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v19) * 1))));
        L48: ;
        L45: ;
        L43: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C7630  int()
    int F_4C7630()
    {
        uint v0 = 0;
        uint v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        int v4 = 0;
        uint v5 = 0;
        int v6 = 0;
        uint v7 = 0;
        int v8 = 0;
        uint v9 = 0;
        int v10 = 0;
        uint v11 = 0;
        int v12 = 0;
        uint v13 = 0;
        int v14 = 0;
        byte v15 = 0;
        int v16 = 0;
        int v17 = 0;
        uint v18 = 0;
        uint v19 = 0;
        uint v20 = 0;
        int v21 = 0;
        uint v22 = 0;
        uint v23 = 0;
        uint v24 = 0;
        int v25 = 0;
        uint v26 = 0;
        int v27 = 0;
        int v28 = 0;
        int v29 = 0;
        int v30 = 0;
        int v31 = 0;
        F_4CEE10((2));
        v0 = ((uint)(M.I32(0x17C6F80U)));
        if (!(M.U32(((uint)(((int)(((uint)(M.I32(0x17C6F80U))) + ((uint)((52)))))))) != 0)) goto L1;
        v1 = M.U32(0x1EABC3CU);
        v2 = ((int)(M.U32(((uint)(M.I32(0x17C6F80U))))));
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C6F80U))) + ((uint)((52))))))), M.U32(0x1EABC3CU));
        M.WI32(0x17C6E14U, ((int)(v1)));
        if (!(v2 != 0)) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(v1, ((uint)((0))), ((uint)(v2)));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v1, ((uint)(v2)));
        v0 = ((uint)(M.I32(0x17C6F80U)));
        v1 = M.U32(0x1EABC3CU);
        L6: ;
        M.WU32(0x1EABC3CU, ((uint)(v1 + (uint)((v2) * 1))));
        L3: ;
        L1: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((14)) * 4)))) != 0)) goto L7;
        v3 = M.U32(0x1EABC3CU);
        v4 = M.I32(v0);
        M.WI32(((uint)(v0 + (uint)(((14)) * 4))), ((int)(M.U32(0x1EABC3CU))));
        M.WI32(0x17C6E14U, ((int)(v3)));
        if (!(v4 != 0)) goto L9;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L11;
        Nat.Memset(v3, ((uint)((0))), ((uint)(v4)));
        goto L12;
        L11: ;
        Nat.DB_LoadXFileData(v3, ((uint)(v4)));
        v0 = ((uint)(M.I32(0x17C6F80U)));
        v3 = M.U32(0x1EABC3CU);
        L12: ;
        M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)((v4) * 1))));
        L9: ;
        L7: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((15)) * 4)))) != 0)) goto L13;
        v5 = M.U32(0x1EABC3CU);
        v6 = M.I32(v0);
        M.WI32(((uint)(v0 + (uint)(((15)) * 4))), ((int)(M.U32(0x1EABC3CU))));
        M.WI32(0x17C6E14U, ((int)(v5)));
        if (!(v6 != 0)) goto L15;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L17;
        Nat.Memset(v5, ((uint)((0))), ((uint)(v6)));
        goto L18;
        L17: ;
        Nat.DB_LoadXFileData(v5, ((uint)(v6)));
        v0 = ((uint)(M.I32(0x17C6F80U)));
        v5 = M.U32(0x1EABC3CU);
        L18: ;
        M.WU32(0x1EABC3CU, ((uint)(v5 + (uint)((v6) * 1))));
        L15: ;
        L13: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((16)) * 4)))) != 0)) goto L19;
        v7 = M.U32(0x1EABC3CU);
        v8 = M.I32(((uint)(v0 + (uint)(((1)) * 4))));
        M.WI32(((uint)(v0 + (uint)(((16)) * 4))), ((int)(M.U32(0x1EABC3CU))));
        M.WI32(0x17C6E14U, ((int)(v7)));
        if (!(v8 != 0)) goto L21;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L23;
        Nat.Memset(v7, ((uint)((0))), ((uint)(v8)));
        goto L24;
        L23: ;
        Nat.DB_LoadXFileData(v7, ((uint)(v8)));
        v0 = ((uint)(M.I32(0x17C6F80U)));
        v7 = M.U32(0x1EABC3CU);
        L24: ;
        M.WU32(0x1EABC3CU, ((uint)(v7 + (uint)((v8) * 1))));
        L21: ;
        L19: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((17)) * 4)))) != 0)) goto L25;
        v9 = M.U32(0x1EABC3CU);
        v10 = M.I32(((uint)(v0 + (uint)(((1)) * 4))));
        M.WI32(((uint)(v0 + (uint)(((17)) * 4))), ((int)(M.U32(0x1EABC3CU))));
        M.WI32(0x17C6E14U, ((int)(v9)));
        if (!(v10 != 0)) goto L27;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L29;
        Nat.Memset(v9, ((uint)((0))), ((uint)(v10)));
        goto L30;
        L29: ;
        Nat.DB_LoadXFileData(v9, ((uint)(v10)));
        v0 = ((uint)(M.I32(0x17C6F80U)));
        v9 = M.U32(0x1EABC3CU);
        L30: ;
        M.WU32(0x1EABC3CU, ((uint)(v9 + (uint)((v10) * 1))));
        L27: ;
        L25: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((18)) * 4)))) != 0)) goto L31;
        v11 = M.U32(0x1EABC3CU);
        v12 = M.I32(((uint)(v0 + (uint)(((1)) * 4))));
        M.WI32(((uint)(v0 + (uint)(((18)) * 4))), ((int)(M.U32(0x1EABC3CU))));
        M.WI32(0x17C6E14U, ((int)(v11)));
        if (!(v12 != 0)) goto L33;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L35;
        Nat.Memset(v11, ((uint)((0))), ((uint)(v12)));
        goto L36;
        L35: ;
        Nat.DB_LoadXFileData(v11, ((uint)(v12)));
        v0 = ((uint)(M.I32(0x17C6F80U)));
        v11 = M.U32(0x1EABC3CU);
        L36: ;
        M.WU32(0x1EABC3CU, ((uint)(v11 + (uint)((v12) * 1))));
        L33: ;
        L31: ;
        F_4CEE70();
        v13 = M.U32(0x1EABC3CU);
        if (!(M.I32(((uint)(v0 + (uint)(((19)) * 4)))) != 0)) goto L37;
        v14 = ((int)(((uint)(M.I32(((uint)(v0 + (uint)(((1)) * 4)))))) + ((uint)(M.I32(((uint)(v0 + (uint)(((2)) * 4))))))));
        v13 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((1))))) & 0xFFFFFFFEU));
        v15 = ((((uint)(((int)(((uint)((2))) * ((uint)(v14)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v16 = ((int)(((uint)((2))) * ((uint)(v14))));
        M.WU32(0x1EABC3CU, v13);
        M.WI32(((uint)(v0 + (uint)(((19)) * 4))), ((int)(v13)));
        M.WU32(0x17C69ACU, v13);
        if (!!((v15 != 0))) goto L39;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L41;
        Nat.Memset(v13, ((uint)((0))), ((uint)(v16)));
        goto L42;
        L41: ;
        Nat.DB_LoadXFileData(v13, ((uint)(v16)));
        v0 = ((uint)(M.I32(0x17C6F80U)));
        v13 = M.U32(0x1EABC3CU);
        L42: ;
        v13 = ((uint)(v13 + (uint)((v16) * 1)));
        M.WU32(0x1EABC3CU, v13);
        L39: ;
        L37: ;
        if (!(M.I32(((uint)(v0 + (uint)(((20)) * 4)))) != 0)) goto L43;
        v13 = ((uint)(((uint)(v13 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v15 = ((((uint)(((int)(((uint)((36))) * ((uint)(M.I32(v0))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v17 = ((int)(((uint)((36))) * ((uint)(M.I32(v0)))));
        M.WU32(0x1EABC3CU, v13);
        M.WI32(((uint)(v0 + (uint)(((20)) * 4))), ((int)(v13)));
        M.WI32(0x17C6ED0U, ((int)(v13)));
        if (!!((v15 != 0))) goto L45;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L47;
        Nat.Memset(v13, ((uint)((0))), ((uint)(v17)));
        goto L48;
        L47: ;
        Nat.DB_LoadXFileData(v13, ((uint)(v17)));
        v0 = ((uint)(M.I32(0x17C6F80U)));
        v13 = M.U32(0x1EABC3CU);
        L48: ;
        v13 = ((uint)(v13 + (uint)((v17) * 1)));
        M.WU32(0x1EABC3CU, v13);
        L45: ;
        L43: ;
        if (!(M.I32(((uint)(v0 + (uint)(((21)) * 4)))) != 0)) goto L49;
        v18 = M.U32(0x17C6940U);
        v19 = ((uint)(((uint)(v13 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WI32(((uint)(v0 + (uint)(((21)) * 4))), ((int)(v19)));
        v30 = ((int)(M.U32(((uint)(v18 + (uint)(((4)) * 4))))));
        M.WU32(0x1EABC3CU, v19);
        M.WU32(0x17C6E20U, v19);
        F_4BAD80(v30);
        v0 = ((uint)(M.I32(0x17C6F80U)));
        v13 = M.U32(0x1EABC3CU);
        L49: ;
        if (!(M.I32(((uint)(v0 + (uint)(((22)) * 4)))) != 0)) goto L51;
        v20 = M.U32(0x17C6940U);
        v13 = ((uint)(((uint)(v13 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        M.WI32(((uint)(v0 + (uint)(((22)) * 4))), ((int)(v13)));
        v15 = ((((uint)(((int)(((uint)((24))) * M.U32(((uint)(v20 + (uint)(((4)) * 4)))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v21 = ((int)(((uint)((24))) * M.U32(((uint)(v20 + (uint)(((4)) * 4))))));
        M.WU32(0x1EABC3CU, v13);
        M.WI32(0x17C6934U, ((int)(v13)));
        if (!!((v15 != 0))) goto L53;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L55;
        Nat.Memset(v13, ((uint)((0))), ((uint)(v21)));
        goto L56;
        L55: ;
        Nat.DB_LoadXFileData(v13, ((uint)(v21)));
        v0 = ((uint)(M.I32(0x17C6F80U)));
        v13 = M.U32(0x1EABC3CU);
        L56: ;
        v13 = ((uint)(v13 + (uint)((v21) * 1)));
        M.WU32(0x1EABC3CU, v13);
        L53: ;
        L51: ;
        if (!(M.I32(((uint)(v0 + (uint)(((23)) * 4)))) != 0)) goto L57;
        v22 = ((uint)(((uint)(v13 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v31 = M.I32(v0);
        M.WU32(0x1EABC3CU, v22);
        M.WI32(((uint)(v0 + (uint)(((23)) * 4))), ((int)(v22)));
        M.WU32(0x17C6DA0U, v22);
        F_4C66E0(v31);
        v0 = ((uint)(M.I32(0x17C6F80U)));
        L57: ;
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((24)) * 4)))) != 0)) goto L59;
        v23 = M.U32(0x17C6940U);
        v24 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((7))))) & 0xFFFFFFF8U));
        M.WI32(((uint)(v0 + (uint)(((24)) * 4))), ((int)(v24)));
        v15 = ((((uint)(((int)(((uint)((8))) * M.U32(((uint)(v23 + (uint)(((4)) * 4)))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v25 = ((int)(((uint)((8))) * M.U32(((uint)(v23 + (uint)(((4)) * 4))))));
        M.WU32(0x1EABC3CU, v24);
        M.WI32(0x17C6D5CU, ((int)(v24)));
        if (!!((v15 != 0))) goto L61;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L63;
        Nat.Memset(v24, ((uint)((0))), ((uint)(v25)));
        goto L64;
        L63: ;
        Nat.DB_LoadXFileData(v24, ((uint)(v25)));
        v0 = ((uint)(M.I32(0x17C6F80U)));
        v24 = M.U32(0x1EABC3CU);
        L64: ;
        M.WU32(0x1EABC3CU, ((uint)(v24 + (uint)((v25) * 1))));
        L61: ;
        L59: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((25)) * 4)))) != 0)) goto L65;
        v26 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((127))))) & 0xFFFFFF80U));
        M.WI32(((uint)(v0 + (uint)(((25)) * 4))), ((int)(v26)));
        v27 = ((int)(((uint)((2))) * ((uint)(M.I32(((uint)(v0 + (uint)(((12)) * 4))))))));
        v15 = ((((uint)(((int)(((uint)((2))) * ((uint)(v27)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v28 = ((int)(((uint)((2))) * ((uint)(v27))));
        M.WU32(0x1EABC3CU, v26);
        M.WI32(0x17C6D54U, ((int)(v26)));
        if (!!((v15 != 0))) goto L67;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L69;
        Nat.Memset(v26, ((uint)((0))), ((uint)(v28)));
        M.WU32(0x1EABC3CU, ((uint)(v26 + (uint)((v28) * 1))));
        goto L70;
        L69: ;
        Nat.DB_LoadXFileData(v26, ((uint)(v28)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v28) * 1))));
        L70: ;
        L67: ;
        L65: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C7B00  int()
    int F_4C7B00()
    {
        uint v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        byte v6 = 0;
        int v7 = 0;
        uint v8 = 0;
        int v9 = 0;
        int v10 = 0;
        v0 = ((uint)(M.I32(0x17C6CACU)));
        v1 = ((int)(M.U32(((uint)(((int)(((uint)(M.I32(0x17C6CACU))) + ((uint)((4))))))))));
        if (!(v1 != 0)) goto L1;
        if (!(((uint)(v1)) == ((uint)((-1))))) goto L3;
        v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v2);
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C6CACU))) + ((uint)((4))))))), v2);
        M.WU32(0x17C6B54U, v2);
        F_4BB090(M.U32(((uint)(M.U32(0x17C6940U) + (uint)(((2)) * 4)))));
        v0 = ((uint)(M.I32(0x17C6CACU)));
        goto L4;
        L3: ;
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C6CACU))) + ((uint)((4))))))), ((uint)(((int)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) >> (28)))))))) + ((uint)(((int)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) & ((uint)((268435455))))))))))));
        L4: ;
        L1: ;
        if (!(M.U32(((uint)(v0 + (uint)(((2)) * 4)))) != 0)) goto L5;
        v3 = M.U32(0x17C6940U);
        v4 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((1))))) & 0xFFFFFFFEU));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), v4);
        v5 = M.U32(((uint)(v3 + (uint)(((3)) * 4))));
        v6 = ((((uint)(((int)(((uint)((2))) * v5)))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v7 = ((int)(((uint)((2))) * v5));
        M.WU32(0x1EABC3CU, v4);
        M.WU32(0x17C69ACU, v4);
        if (!!((v6 != 0))) goto L7;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L9;
        Nat.Memset(v4, ((uint)((0))), ((uint)(v7)));
        goto L10;
        L9: ;
        Nat.DB_LoadXFileData(v4, ((uint)(v7)));
        v0 = ((uint)(M.I32(0x17C6CACU)));
        v4 = M.U32(0x1EABC3CU);
        L10: ;
        M.WU32(0x1EABC3CU, ((uint)(v4 + (uint)((v7) * 1))));
        L7: ;
        L5: ;
        F_4CEE10((2));
        if (!(M.U32(((uint)(v0 + (uint)(((3)) * 4)))) != 0)) goto L11;
        v8 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), v8);
        v9 = ((int)(((uint)(M.U32(v0) << (11)))));
        M.WU32(0x1EABC3CU, v8);
        M.WI32(0x17C6C80U, ((int)(v8)));
        if (!(v9 != 0)) goto L13;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L15;
        Nat.Memset(v8, ((uint)((0))), ((uint)(v9)));
        M.WU32(0x1EABC3CU, ((uint)(v8 + (uint)((v9) * 1))));
        goto L16;
        L15: ;
        Nat.DB_LoadXFileData(v8, ((uint)(v9)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v9) * 1))));
        L16: ;
        L13: ;
        L11: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C7C30  int()
    int F_4C7C30()
    {
        uint v0 = 0;
        uint v1 = 0;
        byte v2 = 0;
        int v3 = 0;
        int v4 = 0;
        v0 = M.U32(0x17C6BE0U);
        if (!(M.U32(((uint)(M.U32(0x17C6BE0U) + (uint)(((1)) * 4)))) != 0)) goto L1;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v2 = ((((uint)(((int)(((uint)((4))) * M.U32(M.U32(0x17C6BE0U)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v3 = ((int)(((uint)((4))) * M.U32(M.U32(0x17C6BE0U))));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(M.U32(0x17C6BE0U) + (uint)(((1)) * 4))), v1);
        M.WU32(0x17C6CB8U, v1);
        if (!!((v2 != 0))) goto L3;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L5;
        Nat.Memset(v1, ((uint)((0))), ((uint)(v3)));
        goto L6;
        L5: ;
        Nat.DB_LoadXFileData(v1, ((uint)(v3)));
        v0 = M.U32(0x17C6BE0U);
        v1 = M.U32(0x1EABC3CU);
        L6: ;
        M.WU32(0x1EABC3CU, ((uint)(v1 + (uint)((v3) * 1))));
        L3: ;
        L1: ;
        M.WU32(0x17C6F98U, ((uint)(v0 + (uint)(((8)) * 1))));
        return F_4B93A0();
        return {};
    }

    // sub_4C7CB0  void __cdecl(int a1)
    void F_4C7CB0(int a1)
    {
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        v1 = a1;
        v2 = ((int)(((uint)((16))) * ((uint)(a1))));
        if (!(((int)(((uint)((16))) * ((uint)(a1)))) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6BE0U), ((uint)((0))), ((uint)(((int)(((uint)((16))) * ((uint)(a1)))))));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6BE0U), ((uint)(((int)(((uint)((16))) * ((uint)(a1)))))));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v2) * 1))));
        L1: ;
        v3 = M.U32(0x17C6BE0U);
        if (!(a1 > (0))) goto L5;
        L7: ;
        M.WU32(0x17C6BE0U, v3);
        F_4C7C30();
        v3 = ((uint)(v3 + (uint)(((16)) * 1)));
        Rt::PreAdd(v1, -1);
        L8: ;
        if ((v1 != 0)) goto L7;
        L9: ;
        L5: ;
        M.WU32(0x17C6BE0U, ((uint)(v3 + (uint)((((int)(0 - v2))) * 1))));
    }

    // sub_4C7D20  void()
    void F_4C7D20()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        byte v3 = 0;
        int v4 = 0;
        uint v5 = 0;
        int v6 = 0;
        uint v7 = 0;
        uint v8 = 0;
        int v9 = 0;
        uint v10 = 0;
        int v11 = 0;
        int v12 = 0;
        uint v13 = 0;
        int v14 = 0;
        v0 = ((uint)(M.I32(0x17C69B0U)));
        if (!(M.U32(((uint)(((int)(((uint)(M.I32(0x17C69B0U))) + ((uint)((4)))))))) != 0)) goto L1;
        v1 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v1);
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C69B0U))) + ((uint)((4))))))), v1);
        M.WU32(0x17C6F98U, v1);
        F_4B9470(M.I32(v0));
        v0 = ((uint)(M.I32(0x17C69B0U)));
        L1: ;
        if (!(M.I32(((uint)(v0 + (uint)(((2)) * 4)))) != 0)) goto L3;
        v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v3 = ((((uint)(((int)(((uint)((12))) * ((uint)(M.I32(v0))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v4 = ((int)(((uint)((12))) * ((uint)(M.I32(v0)))));
        M.WU32(0x1EABC3CU, v2);
        M.WI32(((uint)(v0 + (uint)(((2)) * 4))), ((int)(v2)));
        M.WI32(0x17C6ECCU, ((int)(v2)));
        if (!!((v3 != 0))) goto L5;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L7;
        Nat.Memset(v2, ((uint)((0))), ((uint)(v4)));
        goto L8;
        L7: ;
        Nat.DB_LoadXFileData(v2, ((uint)(v4)));
        v0 = ((uint)(M.I32(0x17C69B0U)));
        v2 = M.U32(0x1EABC3CU);
        L8: ;
        M.WU32(0x1EABC3CU, ((uint)(v2 + (uint)((v4) * 1))));
        L5: ;
        L3: ;
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((3)) * 4)))) != 0)) goto L9;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v3 = ((((uint)(((int)(((uint)((4))) * ((uint)(M.I32(v0))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v6 = ((int)(((uint)((4))) * ((uint)(M.I32(v0)))));
        M.WU32(0x1EABC3CU, v5);
        M.WI32(((uint)(v0 + (uint)(((3)) * 4))), ((int)(v5)));
        M.WI32(0x17C6C30U, ((int)(v5)));
        if (!!((v3 != 0))) goto L11;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L13;
        Nat.Memset(v5, ((uint)((0))), ((uint)(v6)));
        goto L14;
        L13: ;
        Nat.DB_LoadXFileData(v5, ((uint)(v6)));
        v0 = ((uint)(M.I32(0x17C69B0U)));
        v5 = M.U32(0x1EABC3CU);
        L14: ;
        M.WU32(0x1EABC3CU, ((uint)(v5 + (uint)((v6) * 1))));
        L11: ;
        L9: ;
        F_4CEE70();
        if (!(M.I32(((uint)(v0 + (uint)(((5)) * 4)))) != 0)) goto L15;
        v7 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v7);
        M.WI32(((uint)(v0 + (uint)(((5)) * 4))), ((int)(v7)));
        M.WU32(0x17C68E8U, v7);
        F_4BAE00(M.I32(((uint)(v0 + (uint)(((4)) * 4)))));
        v0 = ((uint)(M.I32(0x17C69B0U)));
        L15: ;
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((6)) * 4)))) != 0)) goto L17;
        v8 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v3 = ((((uint)(((int)(((uint)((4))) * ((uint)(M.I32(((uint)(v0 + (uint)(((4)) * 4)))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v9 = ((int)(((uint)((4))) * ((uint)(M.I32(((uint)(v0 + (uint)(((4)) * 4))))))));
        M.WU32(0x1EABC3CU, v8);
        M.WI32(((uint)(v0 + (uint)(((6)) * 4))), ((int)(v8)));
        M.WI32(0x17C6C30U, ((int)(v8)));
        if (!!((v3 != 0))) goto L19;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L21;
        Nat.Memset(v8, ((uint)((0))), ((uint)(v9)));
        goto L22;
        L21: ;
        Nat.DB_LoadXFileData(v8, ((uint)(v9)));
        v0 = ((uint)(M.I32(0x17C69B0U)));
        v8 = M.U32(0x1EABC3CU);
        L22: ;
        M.WU32(0x1EABC3CU, ((uint)(v8 + (uint)((v9) * 1))));
        L19: ;
        L17: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.I32(((uint)(v0 + (uint)(((7)) * 4)))) != 0)) goto L23;
        v10 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v3 = ((((uint)(((int)(((uint)((4))) * ((uint)(M.I32(((uint)(v0 + (uint)(((4)) * 4)))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v11 = ((int)(((uint)((4))) * ((uint)(M.I32(((uint)(v0 + (uint)(((4)) * 4))))))));
        M.WU32(0x1EABC3CU, v10);
        M.WI32(((uint)(v0 + (uint)(((7)) * 4))), ((int)(v10)));
        M.WI32(0x17C6C30U, ((int)(v10)));
        if (!!((v3 != 0))) goto L25;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L27;
        Nat.Memset(v10, ((uint)((0))), ((uint)(v11)));
        goto L28;
        L27: ;
        Nat.DB_LoadXFileData(v10, ((uint)(v11)));
        v0 = ((uint)(M.I32(0x17C69B0U)));
        v10 = M.U32(0x1EABC3CU);
        L28: ;
        M.WU32(0x1EABC3CU, ((uint)(v10 + (uint)((v11) * 1))));
        L25: ;
        L23: ;
        F_4CEE70();
        M.WU32(0x17C6F98U, ((uint)(v0 + (uint)(((8)) * 4))));
        F_4B93A0();
        M.WU32(0x17C6F98U, ((uint)(((int)(((uint)(M.I32(0x17C69B0U))) + ((uint)((36))))))));
        F_4B93A0();
        M.WI32(0x17C6D7CU, ((int)(((uint)(M.I32(0x17C69B0U))) + ((uint)((44))))));
        F_4C6D30();
        M.WI32(0x17C6F34U, ((int)(((uint)(M.I32(0x17C69B0U))) + ((uint)((56))))));
        F_4C6DD0();
        if (!(M.U32(((uint)(((int)(((uint)(M.I32(0x17C69B0U))) + ((uint)((68)))))))) != 0)) goto L29;
        v12 = ((int)(M.U32(((uint)(((int)(((uint)(M.I32(0x17C69B0U))) + ((uint)((64))))))))));
        v13 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((1))))) & 0xFFFFFFFEU));
        v3 = ((((uint)(((int)(((uint)((2))) * ((uint)(v12)))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v14 = ((int)(((uint)((2))) * ((uint)(v12))));
        M.WU32(0x1EABC3CU, v13);
        M.WU32(((uint)(((int)(((uint)(M.I32(0x17C69B0U))) + ((uint)((68))))))), v13);
        M.WI32(0x17C6EB4U, ((int)(v13)));
        if (!!((v3 != 0))) goto L31;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L33;
        Nat.Memset(v13, ((uint)((0))), ((uint)(v14)));
        M.WU32(0x1EABC3CU, ((uint)(v13 + (uint)((v14) * 1))));
        goto L34;
        L33: ;
        Nat.DB_LoadXFileData(v13, ((uint)(v14)));
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v14) * 1))));
        L34: ;
        L31: ;
        L29: ;
    }

    // sub_4C7FE0  int()
    int F_4C7FE0()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        uint v6 = 0;
        uint v7 = 0;
        uint v8 = 0;
        byte v9 = 0;
        int v10 = 0;
        int v11 = 0;
        uint v12 = 0;
        uint v13 = 0;
        uint v14 = 0;
        uint v15 = 0;
        uint v16 = 0;
        uint v17 = 0;
        int v18 = 0;
        uint v19 = 0;
        uint v20 = 0;
        uint v21 = 0;
        int v22 = 0;
        uint v23 = 0;
        int v24 = 0;
        uint v25 = 0;
        uint v26 = 0;
        int v27 = 0;
        uint v28 = 0;
        int v29 = 0;
        uint v30 = 0;
        int v31 = 0;
        uint v32 = 0;
        int v33 = 0;
        uint v34 = 0;
        int v35 = 0;
        uint v36 = 0;
        int v37 = 0;
        uint v38 = 0;
        uint v39 = 0;
        uint v40 = 0;
        uint v41 = 0;
        int v42 = 0;
        int v43 = 0;
        int v44 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6940U);
        Nat.Memset(M.U32(0x17C6940U), ((uint)((0))), ((uint)((628))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6940U), ((uint)((628))));
        v0 = M.U32(0x17C6940U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((628)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6940U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        v3 = M.U32(((uint)(v0 + (uint)(((1)) * 4))));
        v4 = ((uint)(v0 + (uint)(((1)) * 4)));
        M.WU32(0x17C6EECU, ((uint)(v0 + (uint)(((1)) * 4))));
        if (!(v3 != 0)) goto L7;
        if (!(v3 == ((uint)((-1))))) goto L9;
        v5 = M.U32(0x1EABC3CU);
        M.WU32(v4, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v5)));
        Nat.Load_XStringData(v4);
        v0 = M.U32(0x17C6940U);
        goto L10;
        L9: ;
        M.WU32(v4, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v3 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v3 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L10: ;
        L7: ;
        if (!(M.U32(((uint)(v0 + (uint)(((6)) * 4)))) != 0)) goto L11;
        v6 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v6);
        M.WU32(((uint)(v0 + (uint)(((6)) * 4))), v6);
        M.WU32(0x17C6BE0U, v6);
        F_4C7CB0(((int)(M.U32(((uint)(v0 + (uint)(((5)) * 4)))))));
        v0 = M.U32(0x17C6940U);
        L11: ;
        M.WI32(0x17C6CACU, ((int)(((uint)(v0 + (uint)(((13)) * 4))))));
        F_4C7B00();
        v7 = M.U32(0x17C6940U);
        v8 = M.U32(0x1EABC3CU);
        if (!(M.U32(((uint)(M.U32(0x17C6940U) + (uint)(((17)) * 4)))) != 0)) goto L13;
        v8 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v9 = ((((uint)(((int)(((uint)((4))) * M.U32(((uint)(M.U32(0x17C6940U) + (uint)(((13)) * 4)))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v10 = ((int)(((uint)((4))) * M.U32(((uint)(M.U32(0x17C6940U) + (uint)(((13)) * 4))))));
        M.WU32(0x1EABC3CU, v8);
        M.WU32(((uint)(M.U32(0x17C6940U) + (uint)(((17)) * 4))), v8);
        M.WI32(0x17C6BA8U, ((int)(v8)));
        if (!!((v9 != 0))) goto L15;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L17;
        Nat.Memset(v8, ((uint)((0))), ((uint)(v10)));
        goto L18;
        L17: ;
        Nat.DB_LoadXFileData(v8, ((uint)(v10)));
        v7 = M.U32(0x17C6940U);
        v8 = M.U32(0x1EABC3CU);
        L18: ;
        v8 = ((uint)(v8 + (uint)((v10) * 1)));
        M.WU32(0x1EABC3CU, v8);
        L15: ;
        L13: ;
        if (!(M.U32(((uint)(v7 + (uint)(((18)) * 4)))) != 0)) goto L19;
        v11 = ((int)(M.U32(((uint)(v7 + (uint)(((13)) * 4))))));
        v12 = ((uint)(((uint)(v8 + (uint)(((127)) * 1))) & 0xFFFFFF80U));
        M.WU32(0x1EABC3CU, v12);
        M.WU32(((uint)(v7 + (uint)(((18)) * 4))), v12);
        M.WU32(0x17C6964U, v12);
        F_4C6A20(v11);
        v7 = M.U32(0x17C6940U);
        v8 = M.U32(0x1EABC3CU);
        L19: ;
        if (!(M.U32(((uint)(v7 + (uint)(((19)) * 4)))) != 0)) goto L21;
        v13 = ((uint)(((uint)(v8 + (uint)(((3)) * 1))) & 0xFFFFFFFCU));
        v43 = ((int)(M.U32(((uint)(v7 + (uint)(((13)) * 4))))));
        M.WU32(0x1EABC3CU, v13);
        M.WU32(((uint)(v7 + (uint)(((19)) * 4))), v13);
        M.WU32(0x17C69F0U, v13);
        F_4C6B60(v43);
        v7 = M.U32(0x17C6940U);
        L21: ;
        M.WI32(0x17C69B0U, ((int)(((uint)(v7 + (uint)(((20)) * 4))))));
        F_4C7D20();
        M.WI32(0x17C6C6CU, ((int)(((uint)(M.U32(0x17C6940U) + (uint)(((38)) * 4))))));
        F_4C6E60();
        v14 = M.U32(0x17C6940U);
        if (!(M.U32(((uint)(M.U32(0x17C6940U) + (uint)(((53)) * 4)))) != 0)) goto L23;
        v15 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(M.U32(0x17C6940U) + (uint)(((53)) * 4))), v15);
        v16 = M.U32(((uint)(v14 + (uint)(((52)) * 4))));
        M.WU32(0x1EABC3CU, v15);
        M.WU32(0x17C6C0CU, v15);
        F_4B8D10(v16);
        v14 = M.U32(0x17C6940U);
        L23: ;
        if (!(M.U32(((uint)(v14 + (uint)(((62)) * 4)))) != 0)) goto L25;
        v17 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(((uint)(v14 + (uint)(((62)) * 4))), v17);
        v18 = ((int)(M.U32(((uint)(v14 + (uint)(((61)) * 4))))));
        M.WU32(0x1EABC3CU, v17);
        M.WU32(0x17C6D40U, v17);
        F_4C6CC0(v18);
        v14 = M.U32(0x17C6940U);
        L25: ;
        M.WI32(0x17C6970U, ((int)(((uint)(v14 + (uint)(((63)) * 4))))));
        M.WU32(0x17C698CU, ((uint)(v14 + (uint)(((64)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(((int)(((uint)(M.I32(0x17C6970U))) + ((uint)((8))))))));
        F_4BA420();
        M.WU32(0x17C6F98U, ((uint)(M.U32(0x17C6940U) + (uint)(((103)) * 4))));
        F_4B93A0();
        F_4CEE10((2));
        v19 = M.U32(0x17C6940U);
        if (!(M.U32(((uint)(M.U32(0x17C6940U) + (uint)(((104)) * 4)))) != 0)) goto L27;
        v20 = M.U32(((uint)(M.U32(0x17C6940U) + (uint)(((13)) * 4))));
        v21 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v22 = ((int)(((uint)((4))) * ((uint)(((int)(v20 * ((uint)(((int)(((int)(((uint)(v20 + (uint)(((31)) * 1))))) >> (5)))))))))));
        M.WU32(0x1EABC3CU, v21);
        M.WU32(((uint)(M.U32(0x17C6940U) + (uint)(((104)) * 4))), v21);
        M.WI32(0x17C6C80U, ((int)(v21)));
        if (!(v22 != 0)) goto L29;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L31;
        Nat.Memset(v21, ((uint)((0))), ((uint)(((int)(((uint)((4))) * ((uint)(((int)(v20 * ((uint)(((int)(((int)(((uint)(v20 + (uint)(((31)) * 1))))) >> (5))))))))))))));
        goto L32;
        L31: ;
        Nat.DB_LoadXFileData(v21, ((uint)(((int)(((uint)((4))) * ((uint)(((int)(v20 * ((uint)(((int)(((int)(((uint)(v20 + (uint)(((31)) * 1))))) >> (5))))))))))))));
        v19 = M.U32(0x17C6940U);
        v21 = M.U32(0x1EABC3CU);
        L32: ;
        M.WU32(0x1EABC3CU, ((uint)(v21 + (uint)((v22) * 1))));
        L29: ;
        L27: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v19 + (uint)(((105)) * 4)))) != 0)) goto L33;
        v23 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v9 = ((((uint)(((int)(((uint)((4))) * ((uint)(((int)(((int)(((uint)(M.U32(((uint)(v19 + (uint)(((13)) * 4)))) + (uint)(((31)) * 1))))) >> (5))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v24 = ((int)(((uint)((4))) * ((uint)(((int)(((int)(((uint)(M.U32(((uint)(v19 + (uint)(((13)) * 4)))) + (uint)(((31)) * 1))))) >> (5)))))));
        M.WU32(0x1EABC3CU, v23);
        M.WU32(((uint)(v19 + (uint)(((105)) * 4))), v23);
        M.WI32(0x17C6C80U, ((int)(v23)));
        if (!!((v9 != 0))) goto L35;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L37;
        Nat.Memset(v23, ((uint)((0))), ((uint)(v24)));
        goto L38;
        L37: ;
        Nat.DB_LoadXFileData(v23, ((uint)(v24)));
        v19 = M.U32(0x17C6940U);
        v23 = M.U32(0x1EABC3CU);
        L38: ;
        M.WU32(0x1EABC3CU, ((uint)(v23 + (uint)((v24) * 1))));
        L35: ;
        L33: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v19 + (uint)(((106)) * 4)))) != 0)) goto L39;
        v25 = M.U32(((uint)(v19 + (uint)(((143)) * 4))));
        v26 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v27 = ((int)(((uint)((6))) * v25));
        M.WU32(0x1EABC3CU, v26);
        M.WU32(((uint)(v19 + (uint)(((106)) * 4))), v26);
        M.WI32(0x17C6DFCU, ((int)(v26)));
        if (!(((int)(((uint)((6))) * v25)) != 0)) goto L41;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L43;
        Nat.Memset(v26, ((uint)((0))), ((uint)(((int)(((uint)((6))) * v25)))));
        goto L44;
        L43: ;
        Nat.DB_LoadXFileData(v26, ((uint)(((int)(((uint)((6))) * v25)))));
        v19 = M.U32(0x17C6940U);
        v26 = M.U32(0x1EABC3CU);
        L44: ;
        M.WU32(0x1EABC3CU, ((uint)(v26 + (uint)((v27) * 1))));
        L41: ;
        L39: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v19 + (uint)(((107)) * 4)))) != 0)) goto L45;
        v28 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v9 = ((((uint)(((int)(((uint)((4))) * M.U32(((uint)(v19 + (uint)(((144)) * 4)))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v29 = ((int)(((uint)((4))) * M.U32(((uint)(v19 + (uint)(((144)) * 4))))));
        M.WU32(0x1EABC3CU, v28);
        M.WU32(((uint)(v19 + (uint)(((107)) * 4))), v28);
        M.WI32(0x17C6A14U, ((int)(v28)));
        if (!!((v9 != 0))) goto L47;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L49;
        Nat.Memset(v28, ((uint)((0))), ((uint)(v29)));
        goto L50;
        L49: ;
        Nat.DB_LoadXFileData(v28, ((uint)(v29)));
        v19 = M.U32(0x17C6940U);
        v28 = M.U32(0x1EABC3CU);
        L50: ;
        M.WU32(0x1EABC3CU, ((uint)(v28 + (uint)((v29) * 1))));
        L47: ;
        L45: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v19 + (uint)(((108)) * 4)))) != 0)) goto L51;
        v30 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v31 = ((int)(((uint)(((int)(((uint)(((int)((int)(M.U32(((uint)(v19 + (uint)(((8)) * 4)))) - M.U32(((uint)(v19 + (uint)(((7)) * 4))))) / 1)))) - ((uint)((1))))))) << (15)));
        M.WU32(0x1EABC3CU, v30);
        M.WU32(((uint)(v19 + (uint)(((108)) * 4))), v30);
        M.WI32(0x17C6C80U, ((int)(v30)));
        if (!(v31 != 0)) goto L53;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L55;
        Nat.Memset(v30, ((uint)((0))), ((uint)(v31)));
        goto L56;
        L55: ;
        Nat.DB_LoadXFileData(v30, ((uint)(v31)));
        v19 = M.U32(0x17C6940U);
        v30 = M.U32(0x1EABC3CU);
        L56: ;
        M.WU32(0x1EABC3CU, ((uint)(v30 + (uint)((v31) * 1))));
        L53: ;
        L51: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v19 + (uint)(((109)) * 4)))) != 0)) goto L57;
        v32 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v9 = ((((uint)(((int)(((uint)((4))) * ((uint)(((int)(M.U32(((uint)(v19 + (uint)(((143)) * 4)))) * ((uint)(((int)(((uint)(((int)((int)(M.U32(((uint)(v19 + (uint)(((8)) * 4)))) - M.U32(((uint)(v19 + (uint)(((7)) * 4))))) / 1)))) - ((uint)((1))))))))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v33 = ((int)(((uint)((4))) * ((uint)(((int)(M.U32(((uint)(v19 + (uint)(((143)) * 4)))) * ((uint)(((int)(((uint)(((int)((int)(M.U32(((uint)(v19 + (uint)(((8)) * 4)))) - M.U32(((uint)(v19 + (uint)(((7)) * 4))))) / 1)))) - ((uint)((1)))))))))))));
        M.WU32(0x1EABC3CU, v32);
        M.WU32(((uint)(v19 + (uint)(((109)) * 4))), v32);
        M.WI32(0x17C6C80U, ((int)(v32)));
        if (!!((v9 != 0))) goto L59;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L61;
        Nat.Memset(v32, ((uint)((0))), ((uint)(v33)));
        goto L62;
        L61: ;
        Nat.DB_LoadXFileData(v32, ((uint)(v33)));
        v19 = M.U32(0x17C6940U);
        v32 = M.U32(0x1EABC3CU);
        L62: ;
        M.WU32(0x1EABC3CU, ((uint)(v32 + (uint)((v33) * 1))));
        L59: ;
        L57: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v19 + (uint)(((110)) * 4)))) != 0)) goto L63;
        v34 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v9 = ((((uint)(((int)(((uint)((4))) * ((uint)(((int)(M.U32(((uint)(v19 + (uint)(((144)) * 4)))) * ((uint)(((int)(((uint)(((int)((int)(M.U32(((uint)(v19 + (uint)(((8)) * 4)))) - M.U32(((uint)(v19 + (uint)(((7)) * 4))))) / 1)))) - ((uint)((1))))))))))))))) == ((uint)((0)))) ? (byte)1 : (byte)0);
        v35 = ((int)(((uint)((4))) * ((uint)(((int)(M.U32(((uint)(v19 + (uint)(((144)) * 4)))) * ((uint)(((int)(((uint)(((int)((int)(M.U32(((uint)(v19 + (uint)(((8)) * 4)))) - M.U32(((uint)(v19 + (uint)(((7)) * 4))))) / 1)))) - ((uint)((1)))))))))))));
        M.WU32(0x1EABC3CU, v34);
        M.WU32(((uint)(v19 + (uint)(((110)) * 4))), v34);
        M.WI32(0x17C6C80U, ((int)(v34)));
        if (!!((v9 != 0))) goto L65;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L67;
        Nat.Memset(v34, ((uint)((0))), ((uint)(v35)));
        goto L68;
        L67: ;
        Nat.DB_LoadXFileData(v34, ((uint)(v35)));
        v19 = M.U32(0x17C6940U);
        v34 = M.U32(0x1EABC3CU);
        L68: ;
        M.WU32(0x1EABC3CU, ((uint)(v34 + (uint)((v35) * 1))));
        L65: ;
        L63: ;
        F_4CEE70();
        F_4CEE10((2));
        if (!(M.U32(((uint)(v19 + (uint)(((111)) * 4)))) != 0)) goto L69;
        v36 = M.U32(0x1EABC3CU);
        v37 = ((int)(M.U32(((uint)(v19 + (uint)(((143)) * 4))))));
        M.WU32(((uint)(v19 + (uint)(((111)) * 4))), M.U32(0x1EABC3CU));
        M.WI32(0x17C6E14U, ((int)(v36)));
        if (!(v37 != 0)) goto L71;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L73;
        Nat.Memset(v36, ((uint)((0))), ((uint)(v37)));
        goto L74;
        L73: ;
        Nat.DB_LoadXFileData(v36, ((uint)(v37)));
        v19 = M.U32(0x17C6940U);
        v36 = M.U32(0x1EABC3CU);
        L74: ;
        M.WU32(0x1EABC3CU, ((uint)(v36 + (uint)((v37) * 1))));
        L71: ;
        L69: ;
        F_4CEE70();
        if (!(M.U32(((uint)(v19 + (uint)(((112)) * 4)))) != 0)) goto L75;
        v38 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        v44 = ((int)(M.U32(((uint)(v19 + (uint)(((8)) * 4))))));
        M.WU32(0x1EABC3CU, v38);
        M.WU32(((uint)(v19 + (uint)(((112)) * 4))), v38);
        M.WU32(0x17C6BACU, v38);
        F_4C70A0(v44);
        v19 = M.U32(0x17C6940U);
        L75: ;
        if (!(M.U32(((uint)(v19 + (uint)(((113)) * 4)))) != 0)) goto L77;
        v39 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v39);
        M.WU32(((uint)(v19 + (uint)(((113)) * 4))), v39);
        M.WU32(0x17C6B94U, v39);
        F_4C7200(((int)(M.U32(((uint)(v19 + (uint)(((8)) * 4)))))));
        v19 = M.U32(0x17C6940U);
        L77: ;
        M.WI32(0x17C6F80U, ((int)(((uint)(v19 + (uint)(((114)) * 4))))));
        F_4C7630();
        M.WI32(0x17C6EF4U, ((int)(((uint)(M.U32(0x17C6940U) + (uint)(((141)) * 4))))));
        F_4C7290();
        v40 = M.U32(0x17C6940U);
        if (!(M.U32(((uint)(M.U32(0x17C6940U) + (uint)(((155)) * 4)))) != 0)) goto L79;
        v41 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v41);
        M.WU32(((uint)(M.U32(0x17C6940U) + (uint)(((155)) * 4))), v41);
        M.WU32(0x17C6A18U, v41);
        F_4C68E0(M.U32(((uint)(v40 + (uint)(((154)) * 4)))));
        L79: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C8730  int()
    int F_4C8730()
    {
        uint v0 = 0;
        int v1 = 0;
        int v2 = 0;
        uint v3 = 0;
        uint inserted = 0;
        F_4CEE10((0));
        v0 = ((uint)(M.I32(0x17C6E30U)));
        v1 = ((int)(M.U32(((uint)(M.I32(0x17C6E30U))))));
        if (!(M.U32(((uint)(M.I32(0x17C6E30U)))) != 0)) goto L1;
        if (!((((uint)(v1)) != ((uint)((-1)))) && (((uint)(v1)) != ((uint)((-2)))))) goto L3;
        M.WU32(((uint)(M.I32(0x17C6E30U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v1)) - ((uint)((1))))))) >> (28))))))))))))));
        return F_4CEE70();
        L3: ;
        v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v3);
        M.WU32(((uint)(M.I32(0x17C6E30U))), v3);
        M.WU32(0x17C6940U, v3);
        if (!(((uint)(v1)) == ((uint)((-2))))) goto L5;
        inserted = F_4CEEC0();
        goto L6;
        L5: ;
        inserted = ((uint)((0)));
        L6: ;
        Nat.Memset(M.U32(v0), ((uint)((0))), ((uint)((628))));
        M.WI32(0xFEFCECU, ((int)(M.U32(v0))));
        F_4C7FE0();
        F_4CAF90();
        if (!(inserted != 0)) goto L7;
        M.WU32(inserted, M.U32(((uint)(M.I32(0x17C6E30U)))));
        L7: ;
        L1: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C8BC0  void __usercall(unsigned int result@<eax>)
    void F_4C8BC0(uint result)
    {
        int v1 = 0;
        v1 = ((int)(((uint)(((uint)((24))) * result))));
        if (!(((uint)(((uint)((24))) * result)) != 0)) goto L1;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L3;
        Nat.Memset(M.U32(0x17C6A98U), ((uint)((0))), ((uint)(v1)));
        goto L4;
        L3: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A98U), ((uint)(v1)));
        L4: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)((v1) * 1))));
        L1: ;
    }

    // sub_4C8C00  int()
    int F_4C8C00()
    {
        uint v0 = 0;
        uint v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint v5 = 0;
        int v6 = 0;
        if (!(((uint)(M.I32(0x1EBDC44U))) == ((uint)((2))))) goto L1;
        v0 = M.U32(0x17C6A00U);
        M.WU32(M.U32(0x17C6A00U), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((1)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((2)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((3)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((4)) * 4))), ((uint)((0))));
        M.WU32(((uint)(v0 + (uint)(((5)) * 4))), ((uint)((0))));
        goto L2;
        L1: ;
        Nat.DB_LoadXFileData(M.U32(0x17C6A00U), ((uint)((24))));
        v0 = M.U32(0x17C6A00U);
        L2: ;
        M.WU32(0x1EABC3CU, ((uint)(M.U32(0x1EABC3CU) + (uint)(((24)) * 1))));
        F_4CEE10((3));
        v1 = M.U32(v0);
        M.WU32(0x17C6EECU, v0);
        if (!(v1 != 0)) goto L3;
        if (!(v1 == ((uint)((-1))))) goto L5;
        v2 = M.U32(0x1EABC3CU);
        M.WU32(v0, M.U32(0x1EABC3CU));
        M.WI32(0x17C6910U, ((int)(v2)));
        Nat.Load_XStringData(v0);
        v0 = M.U32(0x17C6A00U);
        goto L6;
        L5: ;
        M.WU32(v0, ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v1 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v1 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L6: ;
        L3: ;
        M.WU32(0x17C698CU, ((uint)(v0 + (uint)(((3)) * 4))));
        F_4BA420();
        M.WU32(0x17C698CU, ((uint)(M.U32(0x17C6A00U) + (uint)(((4)) * 4))));
        F_4BA420();
        v3 = M.U32(0x17C6A00U);
        v4 = M.U32(((uint)(M.U32(0x17C6A00U) + (uint)(((5)) * 4))));
        if (!(v4 != 0)) goto L7;
        if (!(v4 == ((uint)((-1))))) goto L9;
        v5 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, v5);
        M.WU32(((uint)(M.U32(0x17C6A00U) + (uint)(((5)) * 4))), v5);
        M.WU32(0x17C6A98U, v5);
        F_4C8BC0(M.U32(((uint)(v3 + (uint)(((2)) * 4)))));
        goto L10;
        L9: ;
        M.WU32(((uint)(M.U32(0x17C6A00U) + (uint)(((5)) * 4))), ((uint)(M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(v4 - (uint)(((1)) * 1))) >> (28)))))))) + ((uint)(((uint)(v4 - (uint)(((1)) * 1))) & ((uint)((268435455))))))));
        L10: ;
        L7: ;
        return F_4CEE70();
        return {};
    }

    // sub_4C8D10  int()
    int F_4C8D10()
    {
        int v0 = 0;
        int v1 = 0;
        uint v2 = 0;
        uint v3 = 0;
        uint v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            F_4CEE10((0));
            v0 = ((int)(M.U32(((uint)(M.I32(0x17C6BC8U))))));
            if (!(M.U32(((uint)(M.I32(0x17C6BC8U)))) != 0)) goto L1;
            if (!((((uint)(v0)) != ((uint)((-1)))) && (((uint)(v0)) != ((uint)((-2)))))) goto L3;
            M.WU32(((uint)(M.I32(0x17C6BC8U))), M.U32(((uint)(((int)(((uint)(((int)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) & ((uint)((268435455))))))) + M.U32(((uint)(((uint)(M.I32(0x1EABBDCU))) + ((uint)(((uint)((8))) * ((uint)(((uint)(((int)(((uint)(v0)) - ((uint)((1))))))) >> (28))))))))))))));
            return F_4CEE70();
            L3: ;
            v2 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, v2);
            M.WU32(((uint)(M.I32(0x17C6BC8U))), v2);
            M.WU32(0x17C6A00U, v2);
            if (!(((uint)(v0)) == ((uint)((-2))))) goto L5;
            F_4CEE10((3));
            v3 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
            M.WU32(0x1EABC3CU, ((uint)(v3 + (uint)(((1)) * 4))));
            F_4CEE70();
            goto L6;
            L5: ;
            v3 = ((uint)((0)));
            L6: ;
            F_4C8C00();
            v4 = ((uint)(M.I32(0x17C6BC8U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6BC8U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v4, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((24))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            Nat.Sys_LeaveCriticalSection();
            if (!(v3 != 0)) goto L7;
            M.WU32(v3, M.U32(((uint)(M.I32(0x17C6BC8U)))));
            L7: ;
            L1: ;
            return F_4CEE70();
            return {};
        }
    }

    // Load_XAsset  int __thiscall(void *this)
    int F_4C8ED0(uint l_this)
    {
        int result = 0;
        uint _t1 = 0;
        result = ((int)(M.U32(M.U32(0x17C6A60U))));
        if (!(M.U32(M.U32(0x17C6A60U)) != 0)) goto L1;
        _t1 = ((uint)(result));
        if (_t1 == 0x1U) goto L4;
        if (_t1 == 0x2U) goto L5;
        if (_t1 == 0x3U) goto L6;
        if (_t1 == 0x4U) goto L7;
        if (_t1 == 0x5U) goto L8;
        if (_t1 == 0x6U) goto L9;
        if (_t1 == 0x7U) goto L10;
        if (_t1 == 0x8U) goto L11;
        if (_t1 == 0x9U) goto L12;
        if (_t1 == 0xAU) goto L13;
        if (_t1 == 0xBU) goto L14;
        if (_t1 == 0xCU) goto L15;
        if (_t1 == 0xDU) goto L16;
        if (_t1 == 0xEU || _t1 == 0xFU) goto L17;
        if (_t1 == 0x10U) goto L18;
        if (_t1 == 0x11U) goto L19;
        if (_t1 == 0x12U) goto L20;
        if (_t1 == 0x13U) goto L21;
        if (_t1 == 0x14U) goto L22;
        if (_t1 == 0x15U) goto L23;
        if (_t1 == 0x16U) goto L24;
        if (_t1 == 0x18U) goto L25;
        if (_t1 == 0x19U) goto L26;
        if (_t1 == 0x1AU) goto L27;
        if (_t1 == 0x1BU) goto L28;
        if (_t1 == 0x1CU) goto L29;
        if (_t1 == 0x1EU) goto L30;
        if (_t1 == 0x1FU) goto L31;
        if (_t1 == 0x24U) goto L32;
        if (_t1 == 0x25U) goto L33;
        if (_t1 == 0x26U) goto L34;
        if (_t1 == 0x27U) goto L35;
        if (_t1 == 0x28U) goto L36;
        if (_t1 == 0x29U) goto L37;
        if (_t1 == 0x2AU) goto L38;
        if (_t1 == 0x2BU) goto L39;
        goto L3;
        L4: ;
        M.WI32(0x17C6F6CU, M.I32(0x17C6B48U));
        return F_4BB710();
        L5: ;
        M.WI32(0x17C6BF0U, M.I32(0x17C6B48U));
        return F_4B7CE0();
        L6: ;
        M.WI32(0x17C6F68U, M.I32(0x17C6B48U));
        return F_4BB950();
        L7: ;
        M.WI32(0x17C69D4U, M.I32(0x17C6B48U));
        return F_4BBEF0();
        L8: ;
        M.WU32(0x17C698CU, ((uint)(M.I32(0x17C6B48U))));
        return F_4BA420();
        L9: ;
        M.WI32(0x17C6B80U, M.I32(0x17C6B48U));
        return F_4B9A30();
        L10: ;
        M.WI32(0x17C6D8CU, M.I32(0x17C6B48U));
        return F_4B98A0();
        L11: ;
        M.WI32(0x17C6A5CU, M.I32(0x17C6B48U));
        return F_4B9BB0();
        L12: ;
        M.WI32(0x17C6D78U, M.I32(0x17C6B48U));
        return F_4BA1F0();
        L13: ;
        M.WU32(0x17C6F98U, ((uint)(M.I32(0x17C6B48U))));
        return F_4B93A0();
        L14: ;
        M.WI32(0x17C6A64U, M.I32(0x17C6B48U));
        return F_4B8830();
        L15: ;
        M.WI32(0x17C6D70U, M.I32(0x17C6B48U));
        return F_4B8350();
        L16: ;
        M.WI32(0x17C6D50U, M.I32(0x17C6B48U));
        return F_4B8170();
        L17: ;
        M.WI32(0x17C69C8U, M.I32(0x17C6B48U));
        return F_4BFB50();
        L18: ;
        M.WI32(0x174686CU, M.I32(0x17C6B48U));
        return F_4BFF80();
        L19: ;
        M.WI32(0x17C68F0U, M.I32(0x17C6B48U));
        return F_4BCB30();
        L20: ;
        M.WI32(0x17C6E2CU, M.I32(0x17C6B48U));
        return F_4BCC20();
        L21: ;
        M.WI32(0x17C6F10U, M.I32(0x17C6B48U));
        return F_4BE9F0();
        L22: ;
        M.WI32(0x17C6C24U, M.I32(0x17C6B48U));
        return F_4BE2E0();
        L23: ;
        M.WI32(0x17C6E30U, M.I32(0x17C6B48U));
        return F_4C8730();
        L24: ;
        M.WI32(0x174687CU, M.I32(0x17C6B48U));
        return F_4BABF0();
        L25: ;
        M.WI32(0x17C6BC8U, M.I32(0x17C6B48U));
        return F_4C8D10();
        L26: ;
        M.WI32(0x17C6960U, M.I32(0x17C6B48U));
        return F_4C1980();
        L27: ;
        M.WU32(0x17C6C64U, ((uint)(M.I32(0x17C6B48U))));
        return F_4C17B0();
        L28: ;
        M.WI32(0x17C6FA8U, M.I32(0x17C6B48U));
        return F_4C1F80();
        L29: ;
        M.WI32(0x17C6DDCU, M.I32(0x17C6B48U));
        return F_4C3C10();
        L30: ;
        M.WI32(0x17C6B88U, M.I32(0x17C6B48U));
        return F_4BCDF0();
        L31: ;
        M.WI32(0x17C6A40U, M.I32(0x17C6B48U));
        return F_4C2230();
        L32: ;
        M.WI32(0x1746878U, M.I32(0x17C6B48U));
        return F_4C59B0();
        L33: ;
        M.WI32(0x17C6B18U, M.I32(0x17C6B48U));
        return F_4C5C50();
        L34: ;
        M.WI32(0x17C6DD8U, M.I32(0x17C6B48U));
        return F_4C65A0();
        L35: ;
        M.WI32(0x17C6ED8U, M.I32(0x17C6B48U));
        return F_4C6280();
        L36: ;
        M.WI32(0x17C6F2CU, M.I32(0x17C6B48U));
        return F_4C2490();
        L37: ;
        M.WI32(0x17C6A38U, M.I32(0x17C6B48U));
        return F_4C54A0();
        L38: ;
        M.WI32(0x17C6E7CU, M.I32(0x17C6B48U));
        return F_4BEBF0();
        L39: ;
        M.WI32(0x17C6BC0U, M.I32(0x17C6B48U));
        return F_4C3E90();
        L3: ;
        goto L2;
        L1: ;
        M.WI32(0x17C6F08U, M.I32(0x17C6B48U));
        return F_4BB4D0();
        L2: ;
        return result;
        return {};
    }

    // sub_4CAAE0  int()
    int F_4CAAE0()
    {
        uint v0 = 0;
        int v1 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            v0 = ((uint)(M.I32(0x17C6BF0U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6BF0U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v0, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((2))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            return Nat.Sys_LeaveCriticalSection();
            return {};
        }
    }

    // sub_4CACE0  int()
    int F_4CACE0()
    {
        uint v0 = 0;
        int v1 = 0;
        int v2 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            v0 = ((uint)(M.I32(0x17C69D4U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C69D4U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v0, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((4))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            v1 = M.I32((_sp + 0x0U));
            if (!((((uint)(((int)(((uint)(M.U8(((uint)(((int)(((uint)(M.I32((_sp + 0x0U)))) + ((uint)((349)))))))))) & ((uint)((1))))))) != ((uint)((0)))) && (((sbyte)(Nat.Stub(0x4C9B10U))) != 0))) goto L1;
            Nat.Stub(0x4CAC00U);
            Nat.Stub(0x4C9C90U);
            L1: ;
            if (!(M.I32(0x194AF64U) != 0)) goto L3;
            Nat.Stub(0x4CA9E0U);
            M.WI32(0x194AF64U, (0));
            L3: ;
            Nat.Stub(0x4CAB20U);
            if (!(((uint)(((int)(((uint)(M.U8(((uint)(((int)(((uint)(v1)) + ((uint)((349)))))))))) & ((uint)((8))))))) != ((uint)((0))))) goto L5;
            Nat.Stub(0x4C9C90U);
            L5: ;
            return Nat.Sys_LeaveCriticalSection();
            return {};
        }
    }

    // sub_4CADB0  int()
    int F_4CADB0()
    {
        uint v0 = 0;
        int v1 = 0;
        int v2 = 0;
        int v3 = 0;
        int v4 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            v0 = ((uint)(M.I32(0x17C6D78U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6D78U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            v1 = ((int)(M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((9))), (_sp + 0x0U)))))) + ((uint)((8))))))))));
            v2 = M.I32((_sp + 0x0U));
            M.WI32(v0, v1);
            if (!(((uint)(v1)) == ((uint)(v2)))) goto L1;
            Nat.Stub(0xA96470U);
            v3 = M.I32(v0);
            if (!(((uint)(v3)) == ((uint)(v2)))) goto L3;
            Nat.Stub(0xA96650U);
            L3: ;
            L1: ;
            return Nat.Sys_LeaveCriticalSection();
            return {};
        }
    }

    // sub_4CAF00  int()
    int F_4CAF00()
    {
        uint v0 = 0;
        int v1 = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            v0 = ((uint)(M.I32(0x17C69C8U)));
            M.WU32(((uint)(((int)(M.U32(((uint)(M.I32(0x17C69C8U)))) + ((uint)((4))))))), ((uint)((1))));
            M.WI32((_sp + 0x0U), M.I32(v0));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WI32(v0, ((int)(M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((15))), (_sp + 0x0U)))))) + ((uint)((8)))))))))));
            return Nat.Sys_LeaveCriticalSection();
            return {};
        }
    }

    // sub_4CAF90  int()
    int F_4CAF90()
    {
        uint v0 = 0;
        int result = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            v0 = ((uint)(M.I32(0x17C6E30U)));
            M.WI32((_sp + 0x0U), ((int)(M.U32(((uint)(M.I32(0x17C6E30U)))))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            M.WU32(v0, M.U32(((uint)(((int)(((uint)(((int)(Nat.DB_AddXAsset(((uint)((21))), (_sp + 0x0U)))))) + ((uint)((8)))))))));
            result = Nat.Sys_LeaveCriticalSection();
            M.WI32(0xFEFCECU, (0));
            return result;
            return {};
        }
    }

    // sub_4CAFF0  int()
    int F_4CAFF0()
    {
        uint v0 = 0;
        int v1 = 0;
        int v2 = 0;
        int i = 0;
        int v4 = 0;
        int result = 0;
        uint _sp = M.StackEnter(0x4U);
        StackGuard _sg{ M, 0x4U };
        {
            v0 = M.U32(0x17C6C64U);
            M.WI32((_sp + 0x0U), ((int)(M.U32(M.U32(0x17C6C64U)))));
            Nat.Sys_EnterCriticalSection(0x2007A5CU);
            v1 = ((int)(Nat.DB_AddXAsset(((uint)((26))), (_sp + 0x0U))));
            v2 = M.I32((_sp + 0x0U));
            M.WU32(v0, M.U32(((uint)(((int)(((uint)(v1)) + ((uint)((8)))))))));
            i = (0);
            L1: ;
            if (!(i < ((int)(M.U32(((uint)(((int)(((uint)(v2)) + ((uint)((176)))))))))))) goto L3;
            v4 = ((int)(M.U32(((uint)(((int)(M.U32(((uint)(((int)(((uint)(v2)) + ((uint)((284)))))))) + ((uint)(((int)(((uint)((4))) * ((uint)(Rt::PostAdd(i, 1))))))))))))));
            L2: ;
            M.WU32(((uint)(((int)(((uint)(v4)) + ((uint)((236))))))), M.U32(v0));
            goto L1;
            L3: ;
            Nat.Stub(0xB42124U);
            result = M.I32(0x2007A68U);
            if (!(M.I32(0x2007A68U) < (0))) goto L4;
            result = ((int)(Nat.Stub(0xB42188U)));
            L4: ;
            if (!(M.I8(0x1876B15U) != 0)) goto L6;
            M.WI8(0x1876B14U, ((sbyte)(1)));
            L8: ;
            result = ((int)(Nat.Stub(0xB42184U)));
            L9: ;
            if ((M.I8(0x1876B15U) != 0)) goto L8;
            L10: ;
            L6: ;
            return result;
            return {};
        }
    }

    // DB_PushStreamPos  int __usercall@<eax>(int block@<eax>)
    int F_4CEE10(int block)
    {
        int v1 = 0;
        int v2 = 0;
        int v3 = 0;
        int result = 0;
        v1 = M.I32(0x1DEA3CCU);
        v2 = M.I32(0x1EBDC44U);
        M.WI32(((uint)(0x203B2CCU + (uint)((((int)(((uint)((2))) * ((uint)(M.I32(0x1DEA3CCU)))))) * 4))), M.I32(0x1EBDC44U));
        v3 = ((int)(((uint)(v1)) + ((uint)((1)))));
        M.WI32(0x1DEA3CCU, v3);
        if (!(((uint)(block)) == ((uint)(v2)))) goto L1;
        result = ((int)(M.U32(0x1EABC3CU)));
        M.WI32(((uint)(0x203B2C0U + (uint)((((int)(((uint)((2))) * ((uint)(v3))))) * 4))), ((int)(M.U32(0x1EABC3CU))));
        goto L2;
        L1: ;
        M.WI32(((uint)(0x203B2A4U + (uint)((v2) * 4))), ((int)(M.U32(0x1EABC3CU))));
        M.WI32(0x1EBDC44U, block);
        result = M.I32(((uint)(0x203B2A4U + (uint)((block) * 4))));
        M.WU32(0x1EABC3CU, ((uint)(result)));
        M.WI32(((uint)(0x203B2C0U + (uint)((((int)(((uint)((2))) * ((uint)(v3))))) * 4))), result);
        L2: ;
        return result;
        return {};
    }

    // DB_PopStreamPos  int __cdecl()
    int F_4CEE70()
    {
        int v0 = 0;
        int result = 0;
        uint _t1 = 0;
        v0 = M.PreAddI32(_t1 = 0x1DEA3CCU, -1);
        if (!!((M.I32(0x1EBDC44U) != 0))) goto L1;
        M.WU32(0x1EABC3CU, ((uint)(M.I32(((uint)(0x203B2C8U + (uint)((((int)(((uint)((2))) * ((uint)(v0))))) * 4)))))));
        L1: ;
        result = M.I32(((uint)(0x203B2CCU + (uint)((((int)(((uint)((2))) * ((uint)(v0))))) * 4))));
        if (!(((uint)(result)) != ((uint)(M.I32(0x1EBDC44U))))) goto L3;
        M.WI32(((uint)(0x203B2A4U + (uint)((M.I32(0x1EBDC44U)) * 4))), ((int)(M.U32(0x1EABC3CU))));
        M.WI32(0x1EBDC44U, result);
        result = M.I32(((uint)(0x203B2A4U + (uint)((result) * 4))));
        M.WU32(0x1EABC3CU, ((uint)(result)));
        L3: ;
        return result;
        return {};
    }

    // DB_InsertPointer  unsigned int __cdecl()
    uint F_4CEEC0()
    {
        uint v0 = 0;
        uint v1 = 0;
        F_4CEE10((3));
        v0 = ((uint)(((uint)(M.U32(0x1EABC3CU) + ((uint)((3))))) & 0xFFFFFFFCU));
        M.WU32(0x1EABC3CU, ((uint)(v0 + ((uint)((4))))));
        F_4CEE70();
        return v0;
        return {};
    }

    // sub_61E860  void()
    void F_61E860()
    {
        uint v0 = 0;
        v0 = M.U32(0x17C6CDCU);
        if (!(M.U32(M.U32(0x17C6CDCU)) != 0)) goto L1;
        M.WI32(0x17C6DC4U, ((int)(M.U32(0x17C6CDCU))));
        F_4B7160();
        M.WI32(v0, ((int)(Nat.DB_FindXAssetHeader(((uint)((11))), M.U32(M.U32(((uint)(M.I32(0x17C6DC4U)))))))));
        L1: ;
    }

    static void write_initial_globals(VMem& m)
    {
        m.WU8(0xFEFCECU + 0U, static_cast<byte>(0xFF));
        m.WU8(0xFEFCECU + 1U, static_cast<byte>(0xFF));
        m.WU8(0xFEFCECU + 2U, static_cast<byte>(0xFF));
        m.WU8(0xFEFCECU + 3U, static_cast<byte>(0xFF));
    }
