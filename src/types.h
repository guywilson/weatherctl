/*
** Common types...
*/
#ifndef _INCL_TYPES
#define _INCL_TYPES

typedef unsigned char		byte;
typedef unsigned long long	bigint;
typedef unsigned long		dword;

typedef enum
{
	Left 		= 0x01,
	Right 		= 0x02,
	Both 		= 0x03
}
Associativity;

typedef enum
{
    Dec         = 0x0A,
    Hex         = 0x0B,
    Bin         = 0x0C,
    Oct         = 0x0D
}
Base;

typedef enum
{
	Plus 		= 0x10,
	Minus 		= 0x11,
	Multiply 	= 0x12,
	Divide 		= 0x13,
	Power 		= 0x14,
	Mod			= 0x15,
	And			= 0x16,
	Or			= 0x17,
	Xor			= 0x18
}
Op;

typedef enum
{
	Open 		= 0x20,
	Close 		= 0x21
}
BraceType;

typedef enum
{
	Pi 			= 0x30,
	C 			= 0x31
}
Const;

typedef enum
{
	Sine 		= 0x40,
	Cosine 		= 0x41,
	Tangent 	= 0x42,
	ArcSine 	= 0x43,
	ArcCosine 	= 0x44,
	ArcTangent 	= 0x45,
	SquareRoot 	= 0x46,
	Logarithm 	= 0x47,
	NaturalLog  = 0x48,
	Factorial	= 0x49,
	Memory		= 0x4A
}
Func;

#endif
