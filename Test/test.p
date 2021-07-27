#pragma semicolon 1

native opaque(v);

const highest_set = 1 << (cellbits - 1);

forward test_Arithmetic();
public test_Arithmetic() {
    if (1 + opaque(2) != opaque(3)) return 0;
    if (highest_set + opaque(highest_set | 1) != opaque(1)) return 0;
    if (4 - opaque(1) != opaque(3)) return 0;
    if (1 - opaque(4) != opaque(-3)) return 0;
    if (5 & opaque(3) != opaque(1)) return 0;
    if (5 | opaque(3) != opaque(7)) return 0;
    if (5 ^ opaque(3) != opaque(6)) return 0;
    if (!opaque(3) != !!opaque(0)) return 0;
    if (-opaque(1) != opaque(-1)) return 0;
    if (~opaque(0) != opaque(-1)) return 0;
    if (opaque(1) << 1 != opaque(2)) return 0;
    if (opaque(2) >> 1 != opaque(1)) return 0;
    if (opaque(1) << opaque(1) != opaque(2)) return 0;
    if (opaque(2) >> opaque(1) != opaque(1)) return 0;
    if (~opaque(0) >> opaque(1) != ~opaque(0)) return 0;
    if (~opaque(0) >>> opaque(1) != ~highest_set) return 0;
    if (opaque(2) * opaque(3) != opaque(6)) return 0;
    if (opaque(-2) * opaque(3) != opaque(-6)) return 0;
    if (opaque(-2) * opaque(-3) != opaque(6)) return 0;

    new v, w;

    // eq, taken
    v = 0;
    if (opaque(1) == opaque(1)) v = 1; else v = 0;
    if (!v) return 0;

    // eq, not taken
    v = 0;
    if (opaque(1) == opaque(2)) v = 0; else v = 1;
    if (!v) return 0;

    // neq, taken
    v = 0;
    if (opaque(1) != opaque(2)) v = 1; else v = 0;
    if (!v) return 0;

    // neq, not taken
    v = 0;
    if (opaque(1) != opaque(1)) v = 0; else v = 1;
    if (!v) return 0;

    // sless, taken
    v = 0;
    if (opaque(-1) < opaque(1)) v = 1; else v = 0;
    if (!v) return 0;

    // sless, not taken
    v = 0;
    if (opaque(1) < opaque(-1)) v = 0; else v = 1;
    if (!v) return 0;

    // sleq, taken
    v = 0;
    if (opaque(-1) <= opaque(-1)) v = 1; else v = 0;
    if (!v) return 0;

    // sleq, not taken
    v = 0;
    if (opaque(1) <= opaque(-1)) v = 0; else v = 1;
    if (!v) return 0;

    // sgrtr, taken
    v = 0;
    if (opaque(1) > opaque(-1)) v = 1; else v = 0;
    if (!v) return 0;

    // sgrtr, not taken
    v = 0;
    if (opaque(-1) > opaque(1)) v = 0; else v = 1;
    if (!v) return 0;

    // sgeq, taken
    v = 0;
    if (opaque(-1) >= opaque(-1)) v = 1; else v = 0;
    if (!v) return 0;

    // sgeq, not taken
    v = 0;
    if (opaque(-1) >= opaque(1)) v = 0; else v = 1;
    if (!v) return 0;

    v = opaque(0);
    w = opaque(2);
    if (--w != ++v) return 0;

    v = opaque(2);
    w = opaque(0);
    if (++w != --v) return 0;

    return 1;
}

inc_i(&v)
    ++v;

dec_i(&v)
    --v;

inc(v)
    return ++v;

dec(v)
    return --v;

inc_3(&v) {
    v += 2;
    v += opaque(1);
}

add_i(a, b)
    return a + b;

forward test_Indirect();
public test_Indirect() {
    new v;

    // inc_i
    v = opaque(0);
    inc_i(v);
    if (v != 1) return 0;

    // dec_i
    v = opaque(0);
    dec_i(v);
    if (v != -1) return 0;

    // inc
    v = opaque(0);
    if (inc(v) != 1) return 0;

    // dec
    v = opaque(0);
    if (dec(v) != -1) return 0;

    // lref_s / sref_s
    v = opaque(0);
    inc_3(v);
    if (v != 3) return 0;

    if (add_i(opaque(1), opaque(2)) != 3) return 0;

    return 1;
}

inc_arr(a[], c)
    for (new i = 0; i < c; ++i)
        ++a[i];

inc_arr_i(a[], c)
    for (new i = 0; i < c; ++i)
        a[i] = opaque(a[i] + 1);

sum_arr(const a[], c) {
    new sum = 0;
    for (new i = 0; i < c; ++i)
        sum += a[i];
    return sum;
}

memcmp(const a[], const b[], c) {
    for (new i = 0; i < c; ++i)
        if (a[i] != b[i])
            return true;
    return false;
}

memcpy(dst[], const src[], c)
    for (new i = 0; i < c; ++i)
        dst[i] = src[i];

store_scaled(a[], i, v)
    a[i << 1] = v;

forward test_Array();
public test_Array() {
    new arr[] = [0, 0, 0];
    new test[] = [1, 1, 1];
    inc_arr(arr, 3);
    if (0 != memcmp(arr, test, 3)) return 0;
    new test2[] = [2, 2, 2];
    inc_arr_i(arr, 3);
    if (0 != memcmp(arr, test2, 3)) return 0;
    if (sum_arr(test2, 3) != 6) return 0;
    memcpy(arr, test, 3);
    if (0 != memcmp(arr, test, 3)) return 0;
    store_scaled(arr, 1, 0);
    if (arr[2] != 0) return 0;

    return 1;
}

forward test_ArrayOverindex();
public test_ArrayOverindex() {
    new arr[] = [0, 0, 0];
    // we use memcmp for this to not accidentally trash data for other tests
    memcmp(arr, arr, 999999);
}

forward test_Switch();
public test_Switch() {
    switch (opaque(1)) {
        case 0: return 0;
        case 1: return 1;
        default: opaque(0);
    }
    return 0;
}

forward test_SwitchBreak();
public test_SwitchBreak() {
    switch (opaque(1)) {
        case 0: return 0;
        case 1: opaque(0);
        default: return 0;
    }
    return 1;
}

forward test_SwitchDefault();
public test_SwitchDefault() {
    switch (opaque(2)) {
        case 1: opaque(0);
        default: return 1;
    }
    return 0;
}

forward test_SwitchOnlyDefault();
public test_SwitchOnlyDefault() {
    switch (opaque(2)) {
        default: return 1;
    }
    return 0;
}

forward test_Div();
public test_Div() {
    if(opaque(4) / opaque(2) != 2) return 0;
    if(opaque(5) / opaque(2) != 2) return 0;
    if(opaque(-4) / opaque(2) != -2) return 0;
    if(opaque(-5) / opaque(2) != -3) return 0;
    if(opaque(4) / opaque(-2) != -2) return 0;
    if(opaque(5) / opaque(-2) != -3) return 0;
    if(opaque(-4) / opaque(-2) != 2) return 0;
    if(opaque(-5) / opaque(-2) != 2) return 0;
    return 1;
}

forward test_DivZero();
public test_DivZero() {
    return 5 / opaque(0);
}

// we don't have numargs / getarg / setarg, they should be implemented as stock instead
vararg(...)
    return 1;

forward test_VarArgs();
public test_VarArgs() {
    return vararg(1, 2, 3, 4, 5);
}
