#pragma semicolon 1
native five();
add_one(o) {
    switch (o) {
        case 5: return 6;
        default: return -1;
    }
    return 0;
}

forward square(v);
public square(v)
    return v * v;

forward get_two(&two);
public get_two(&two)
    two = 2;

main()
	return add_one(five()) + 1;

