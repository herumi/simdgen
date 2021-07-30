# simdgenの仕様と内部

## SIMDのループ
- ループ回数nに対して16(*UNROLL)ずつ計算するメインの部分と端数の部分をコード生成
- 与えられた文字列式`log(exp(x)+1)`を計算するSIMDループを生成する
  - 関数計算に必要な初期化はループの外側で行う
  - レジスタの退避・復元はループの外側で一度だけ
  - AVX-512/SVE両対応

## ファイル構成

```
include/simdgen/simdgen.h ; 公開ヘッダファイル
src/const.hpp             ; 関数内で利用する定数
   /tokenlist.hpp         ; 構文解析後のデータを保持
   /parser.hpp            ; 四則演算+αの構文解析
   /gen.hpp               ; 構文解析の結果に応じてコード生成(共通部分)
   /main.cpp              ; APIの実装
   x64/main.hpp           ; AVX-512用コード生成
   aarch64/main.hpp       ; SVE用コード生成
```

行数(2021/7/30)
wc -l
```
   96 const.hpp
  305 gen.hpp
  238 parser.hpp
  252 tokenlist.hpp
  260 x64/main.hpp
  252 aarch64/main.hpp
```

## tokenlist.hpp

### ValueType
値(Value)の種類
- Const ; 定数
- Var   ; 変数
- Op    ; 演算子
- Func  ; 関数

### OpType
演算子の種類
- Add
- Sub
- Mul
- Div

### FuncType
関数の種類
- Inv
- Exp
- Log
- Tanh

### Value
`Value`クラスは値を持つ

### Index
- クラスTのインスタンスを登録順に保持する
- 変数名`x`, `xxx`, `y`などを0, 1, 2, ...にする
- 定数`1.0`, `3.141`などを0, 1, 2, ...にする

### TokenList
- 構文解析したValueの列を持つ(ValueVec vv)
- 変数名についてのindex
- どの関数を利用したか
  - 登録する定数が変わるので
- 一つの式の計算に必要なレジスタ数

## parser.hpp
四則演算+αの構文解析

- parseFloat ; float定数
  - ちょっとめんどい
  - 1, 1.0, .1, 1., -1.0, 1e3, 1.2e-3, 1e+4, etc.
- parseVar ; 変数名

### parse()
文字列式からTokenListを生成する

- addSub ; `mulDiv (('+'|'-') mulDiv)*`
- mulDiv ; `expr ('*'|'/') expr)*`
- expr   ; `var|num|'('addSub')'|func(addSub)`
- `var`  ; 変数名
- `num`  ; float定数

## gen.hpp
- FuncInfo ; その関数に必要な一時レジスタと一時マスクレジスタの数
- IndexRange ; 一時変数のどこまで利用したか
- GeneratorBase ; コード生成のうち共通部分

### execOneLoop(const TokenList& tl)
与えられたtlに基づいて一つ分のSIMDコードを生成する

## x64 (AVX-512)
- freeTbl ; AVX-512レジスタzmm0, ..., zmm31のうち自由に使えるもの
- saveTbl ; 退避すべきレジスタ

### Generator
コード生成本体

```
    Label dataL = L();   // data領域
    updateConstIdx(tl);  // TokenListで利用される定数を設定
    for (size_t i = 0; i < constN_; i++) {
        dd(constIdx_.getVal(i)); // data領域に展開
    }
    setSize(dataSize);           // dataSize終わり
    addr_ = getCurr<SgFuncFloat1*>(); // ここからコード開始
    // スタックフレーム構築
    StackFrame sf(this, 3, 1 | UseRCX, keepN_ * simdByte_); // keepN * simdByteだけスタック確保
    // store regs
    for (int i = 0; i < keepN_; i++) {
        vmovups(ptr[rsp + i * simdByte_], Zmm(saveTbl[i])); // 退避すべきレジスタをスタックに退避
    }
    // とりあえず関数の形はfunc(float *dst, const *src, size_t n)に限定
    const Reg64& dst = sf.p[0];
    const Reg64& src = sf.p[1];
    const Reg64& n = sf.p[2];
    dataReg_ = sf.t[0];
    mov(dataReg_, (size_t)dataL.getAddress());
    gen_setConst();  // 必要な定数をレジスタに設定
    test(n, n);
    Label mod16, exit;
    mov(ecx, n);
    and_(n, ~15u);
    jz(mod16, T_NEAR); // ループ変数nが16未満ならmod16へジャンプ
    puts("execOneLoop lp");
Label lp = L();                           // ループの先頭
    vmovups(Zmm(getVarIdx(0)), ptr[src]); // srcからレジスタに読み込み
    execOneLoop(tl);                      // ループ1回分(将来unroll予定)のコード生成
    vmovups(ptr[dst], Zmm(getTmpIdx(0))); // 計算結果をdstに書き込み
    add(src, 64);                         // ポインタ更新
    add(dst, 64);
    sub(n, 16);
    jnz(lp, T_NEAR);                      // n > 16である限りループ
L(mod16);
    puts("execOneLoop mod16");            // 残り
    and_(ecx, 15);                        // n &= 15
    jz(exit, T_NEAR);
    mov(eax, 1);
    shl(eax, cl);
    sub(eax, 1);
    kmovd(k1, eax);                      // マスクレジスタk1に(1 << n) - 1を設定
    vmovups(Zmm(getVarIdx(0))|k1|T_z, ptr[src]); // srcからn個読み込み
    execOneLoop(tl);                        // 残りのコード生成
    vmovups(ptr[dst]|k1, Zmm(getTmpIdx(0)));// 結果の書き込み
L(exit);
    // restore regs
    for (int i = 0; i < keepN_; i++) {      // スタックからレジスタ復元
        vmovups(Zmm(saveTbl[i]), ptr[rsp + i * simdByte_]);
    }
```

### 演算子

```
void gen_copy(int dst, int src)
{
    if (debug) printf("vmovaps z%d, z%d\n", dst, src);
    vmovaps(Zmm(dst), Zmm(src));
}
void gen_add(int dst, int src1, int src2)
{
    if (debug) printf("vaddps z%d, z%d, z%d\n", dst, src1, src2);
    vaddps(Zmm(dst), Zmm(src1), Zmm(src2));
}
void gen_sub(int dst, int src1, int src2)
{
    if (debug) printf("vsubps z%d, z%d, z%d\n", dst, src1, src2);
    vsubps(Zmm(dst), Zmm(src1), Zmm(src2));
}
void gen_mul(int dst, int src1, int src2)
{
    if (debug) printf("vmulps z%d, z%d, z%d\n", dst, src1, src2);
    vmulps(Zmm(dst), Zmm(src1), Zmm(src2));
}
```

### Exp

```
void gen_exp(int inout)
{
    if (debug) printf("exp z%d\n", inout);
    const Zmm log2(getFloatIdx(g_expTbl.log2));     // `float log2`を格納しているレジスタ番号
    const Zmm log2_e(getFloatIdx(g_expTbl.log2_e));
    const Zmm tbl[] = {
        Zmm(getFloatIdx(g_expTbl.coef[0])),
        Zmm(getFloatIdx(g_expTbl.coef[1])),
        Zmm(getFloatIdx(g_expTbl.coef[2])),
        Zmm(getFloatIdx(g_expTbl.coef[3])),
        Zmm(getFloatIdx(g_expTbl.coef[4])),
    };
    const Zmm t0(inout);                        // 入力レジスタ
    IndexRangeManager ftr(funcTmpReg_);
    const Zmm t1(ftr.allocIdx());               // expのコード生成内で利用する一時レジスタ
    const Zmm t2(ftr.allocIdx());

    vmulps(t0, log2_e);                         // 実際のexpコード生成
    vrndscaleps(t1, t0, 0); // n = round(x)
    vsubps(t0, t1); // a
    vmulps(t0, log2);
    vmovaps(t2, tbl[4]);
    vfmadd213ps(t2, t0, tbl[3]);
    vfmadd213ps(t2, t0, tbl[2]);
    vfmadd213ps(t2, t0, tbl[1]);
    vfmadd213ps(t2, t0, tbl[0]);
    vfmadd213ps(t2, t0, tbl[0]);
    vscalefps(t0, t2, t1); // t2 * 2^t1
}
```

## SVE (AArch64)

### Generator
```
    adr(dataReg_, dataL);
    ptrue(p0.s);
    // store regs
    for (int i = 0; i < keepN_; i++) {
        sub(sp, sp, 64);
        st1w(ZReg(saveTbl[i]).s, p0, ptr(sp));
    }
    const XReg& dst = x0;
    const XReg& src = x1;
    const XReg& n = x2;
    gen_setConst();

    Label skip;
    b(skip);
Label lp = L();
    ld1w(ZReg(getVarIdx(0)).s, p0, ptr(src));
    add(src, src, 64);
    execOneLoop(tl);
    st1w(ZReg(getTmpIdx(0)).s, p0, ptr(dst));
    add(dst, dst, 64);
    sub(n, n, 16);
L(skip);
    cmp(n, 16);
    bge(lp);

    Label cond;
    mov(tmpX_, 0);
    b(cond);
Label lp2 = L();
    ld1w(ZReg(getVarIdx(0)).s, p1, ptr(src, tmpX_, LSL, 2));
    execOneLoop(tl);
    st1w(ZReg(getTmpIdx(0)).s, p1, ptr(dst, tmpX_, LSL, 2));
    incd(tmpX_);
L(cond);
    whilelt(p1.s, tmpX_, n);
    b_first(lp2);

    // restore regs
    for (int i = 0; i < keepN_; i++) {
        ld1w(ZReg(saveTbl[i]).s, p0, ptr(sp));
        add(sp, sp, 64);
    }
    ret();
    ready();
```