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
- Inv : inverse
- Exp : exp
- Log : log
- Cosh : cosh
- Tanh : tanh
- DebugFunc : for debug
- RedSum : sum

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

detectUnrollN(tl);   // unroll回数を決定

setSize(0);
for (uint32_t i = 0; i < constTblMem_.size(); i++) {
    const SimdArray& v = constTblMem_.getVal(i);
    for (size_t j = 0; j < v.N; j++) {
        dd(v.get32bit(j));                            // 利用する定数をdata領域に展開
    }
}
for (uint32_t i = 0; i < constMem_.size(); i++) {
    dd(constMem_.getVal(i));                          // 64バイト単位のdata
}
if (getSize() > dataSize) {
    throw cybozu::Exception("bad data size") << getSize();
}
setSize(dataSize);        // dataSize終わり
addr_ = getCurr<void*>(); // ここからコード開始
if (opt.break_point) int3();
{
    int keepN = 0;
    if (totalN_ > maxFreeN) keepN = totalN_ - maxFreeN;
    // スタックフレーム構築
    StackFrame sf(this, 3, 1 | UseRCX | UseRDX, keepN * simdByte_);
    // store regs
    // 退避すべきレジスタをスタックに退避
    for (int i = 0; i < keepN; i++) {
        vmovups(ptr[rsp + i * simdByte_], Zmm(maxFreeN + i));
    }
    Reg64 dst, src, n;
    if (reduceFuncType_ >= 0) {
        // float func(const float *src, size_t n);
        src = sf.p[0];
        n = sf.p[1];
    } else {
        // void func(float *dst, const float *src, size_t n);
        dst = sf.p[0];
        src = sf.p[1];
        n = sf.p[2];
    }
    dataReg_ = sf.t[0];
    mov(dataReg_, (size_t)dataL.getAddress());
    gen_setConst();
    if (reduceFuncType_ >= 0) {
        LP_(i, unrollN_) {
            Zmm red(getReduceVarIdx() + i);
            vxorps(red, red);
        }
    }

    Label cmp1L, cmp2L, exitL;
    jmp(cmp1L, T_NEAR);
    puts("execOneLoop lp");
Label lp1 = L(); // while (n >= 16 * unrollN_)
    LP_(i, unrollN_) vmovups(Zmm(getVarIdx(i)), ptr[src + i * simdByte_]);// srcからレジスタに読み込み
    execOneLoop(tl, unrollN_);                                            // ループunrollN_回分のコード生成
    LP_(i, unrollN_) outputOne(dst, i); // 計算結果をdstに書き込み
    add(src, 64 * unrollN_);            // ポインタ更新
    if (reduceFuncType_ < 0) add(dst, 64 * unrollN_);
    sub(n, 16 * unrollN_);
L(cmp1L);
    cmp(n, 16  * unrollN_);
    jge(lp1, T_NEAR);

    // 残り
    if (unrollN_ > 1) {
        jmp(cmp2L, T_NEAR);
    Label lp2 = L();
        vmovups(Zmm(getVarIdx(0)), ptr[src]);
        execOneLoop(tl, 1);
        outputOne(dst, 0);
        add(src, 64);
        if (reduceFuncType_ < 0) add(dst, 64);
        sub(n, 16);
    L(cmp2L);
        cmp(n, 16);
        jge(lp2, T_NEAR);
    }

    mov(ecx, n);
    test(ecx, ecx);
    jz(exitL, T_NEAR);

    mov(tmp32_, 1);
    shl(tmp32_, cl);
    sub(tmp32_, 1);
    kmovd(k1, tmp32_); // マスクレジスタk1に(1 << n) - 1を設定
    vmovups(Zmm(getVarIdx(0))|k1|T_z, ptr[src]);// srcからn個読み込み
    execOneLoop(tl, 1);   // 残りのコード生成
    outputOne(dst, 0, k1);// 結果の書き込み
L(exitL);
    if (reduceFuncType_ >= 0) {
        reduceAll();
    }
    // restore regs
    for (int i = 0; i < keepN; i++) { // スタックからレジスタ復元
        vmovups(Zmm(maxFreeN + i), ptr[rsp + i * simdByte_]);
    }
```

### 演算子

```
void gen_copy(int dst, int src)
{
    vmovaps(Zmm(dst), Zmm(src));
}
void gen_add(int dst, int src1, int src2)
{
    vaddps(Zmm(dst), Zmm(src1), Zmm(src2));
}
void gen_sub(int dst, int src1, int src2)
{
    vsubps(Zmm(dst), Zmm(src1), Zmm(src2));
}
void gen_mul(int dst, int src1, int src2)
{
    vmulps(Zmm(dst), Zmm(src1), Zmm(src2));
}
```

### 関数生成コード(`gen_func(int inout, int n)`)の中でのレジスタの扱い

inoutは入力データのSIMDレジスタ番号。
inout, inout+1, ..., inout+n-1にデータが入っている。nはループアンロール回数。

汎用レジスタ

プログラム中で自由に使ってよい汎用レジスタはtmp32_(32bit)とtmp64_(64bit)のみ。

SIMDレジスタ

```
const auto inp = getInputRegVec(inout, n);
```
これでinp[0], ..., inp[n-1]は入力SIMDレジスタとなる。
関数を抜けるときはこれらのレジスタが戻り値となる。

```
IndexRangeManager ftr(funcTmpReg_);
const ZmmVec t = getTmpRegVec(ftr, n);
```

これで関数内で一時的に利用できるSIMDレジスタがn個割り当てられる。
t[0], ..., t[n-1]を利用できる。

```
Zmm t(ftr.allocIdx());
```
これで関数内で一時利用できるSIMDレジスタが1個割り当てられる。

マスクレジスタ

```
IndexRangeManager ftm(funcTmpMask_);
const auto mask = getTmpMaskVec(ftm, n);
```
これで関数内で一時利用できるマスクレジスタがn個割り当てられる。

### 定数の扱い

- `getFloatIdx(float x)`
  - xの値をSIMDレジスタに設定し、そのレジスタ番号を返す。関数プロローグで一度だけ設定される。このレジスタの値を変更してはいけない。
  - 複数回呼び出しても同じレジスタ番号を返す。
- `getConstTblIdx(const void *p, size_t byteSize)`
  - `p[0, byteSize)`のデータをSIMDレジスタに設定し、そのレジスタ番号を返す。関数プロローグで一度だけ設定される。このレジスタの値を変更してはいけない。
  - 複数回呼び出しても同じレジスタ番号を返す。
- `getConstOffsetToDataReg(uint32_t x)`
  - xの値をメモリに設定し、dataReg_からのbyteオフセットを返す。SIMDレジスタには読まれない。必要に応じて自分で設定する。
- `getConstTblOffsetToDataReg(const void *p, size_t byteSize)`
  - `p[0, byteSize)`のデータをメモリに設定し、dataReg_からのbyteオフセットを返す。SIMDレジスタには読まれない。必要に応じて自分で設定する。

- `setInt(const Zmm& z, uint32_t u)`
  - zレジスタに値uをブロードキャスト設定する。
- `setFloat(const Zmm& z, float f)`
  - zレジスタに値fをブロードキャスト設定する。


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
        if (debug) printf("saveRegBegin=%d saveRegEnd=%d totalN_=%d\n", saveRegBegin, saveRegEnd, totalN_);
        const int saveN = std::min(saveRegEnd, totalN_);
        for (int i = saveRegBegin; i < saveN; i++) {
            sub(sp, sp, 64);
            st1w(ZReg(i).s, p0, ptr(sp));
        }
        const int saveMaskN = funcTmpMask_.getMax();
        for (int i = savePredBegin; i < saveMaskN; i++) {
            sub(sp, sp, 16);
            str(PReg(i).s, ptr(sp));
        }

        XReg dst = x0, src = x1, n = x2;
        if (reduceFuncType_ >= 0) {
            // dst is not used
            src = x0;
            n = x1;
        }
        gen_setConst();
        if (reduceFuncType_ >= 0) {
            LP_(i, unrollN_) {
                ZRegS red(getReduceVarIdx() + i);
                mov(red, 0);
            }
        }

        Label skipL, exitL;
        b(skipL);
    Label lp = L();
        LP_(i, unrollN_) ldr(ZReg(getVarIdx(i)), ptr(src, i));
        add(src, src, 64 * unrollN_);
        execOneLoop(tl, unrollN_);
        LP_(i, unrollN_) outputOne(dst, i);
        if (reduceFuncType_ < 0) add(dst, dst, 64 * unrollN_);
        sub(n, n, 16 * unrollN_);
    L(skipL);
        cmp(n, 16 * unrollN_);
        bge(lp);

        cmp(n, 0);
        beq(exitL);

        Label cond;
        mov(loop_i_, 0);
        b(cond);
    Label lp2 = L();
        ld1w(ZReg(getVarIdx(0)).s, p1, ptr(src, loop_i_, LSL, 2));
        execOneLoop(tl, 1);
        outputOne(dst, 0, &loop_i_);
        incw(loop_i_);
    L(cond);
        whilelt(p1.s, loop_i_, n);
        b_first(lp2);
    L(exitL);

        if (reduceFuncType_ >= 0) {
            reduceAll();
        }

        // restore regs
        for (int i = savePredBegin; i < saveMaskN; i++) {
            ldr(PReg(saveMaskN + savePredBegin - 1 - i).s, ptr(sp));
            add(sp, sp, 16);
        }
        for (int i = saveRegBegin; i < saveN; i++) {
            ld1w(ZReg(saveN + saveRegBegin - 1 - i).s, p0, ptr(sp));
            add(sp, sp, 64);
        }
        ret();
        ready();
```
