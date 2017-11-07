# Z3 Code Generator

## Description

A program for generating code sequences ("program synthesis") in a specified Instruction Set such that the generated code sequence models some user defined function. The program works by specifying a series of constraints using the [Z3 SMT solver](https://github.com/Z3Prover/z3) and attempting to find a solution which satisfies all these constraints.

The program is an expanded version of the approach outlined by Dennis Yurichev in section 8.1 of [Quick introduction into SAT/SMT solvers and symbolic execution](https://yurichev.com/writings/SAT_SMT_draft-EN.pdf). I think that gives a good overview of how the program works so I won't repeat it here.

## Using the program

In `codegen.c` there is a function `ValueType TargetFunc(const ValueType x)` which is used to drive the generation. The program will generate random inputs and then call the function to get the corresponding output. These values are then used to drive the generation "chains" as described in Dennis's paper. 

If you want to generate code for some function then simply update the `TargetFunc` definition to whatever you like. 

With a little effort the program can also be expanded to handle multiple input values.

Once the program has completed you should see output like this:

```
Try with 2 instructions...
  solver.check() call completed: 30 ms
  unsatifiable
Try with 3 instructions...
  solver.check() call completed: 135 ms
  unsatifiable
Try with 4 instructions...
  solver.check() call completed: 504 ms
  unsatifiable
Try with 5 instructions...
  solver.check() call completed: 4734 ms
  satisified!

Generated code:
  r0 = <input>
  r1 = set 0x3
  r2 = gt r0 r1
  r3 = xor r0 r2
  r4 = sub r2 r3

Testing with random values...
  10000 / 10000 passed

Press any key to continue . . .
```

**Warning**: if you specify a function which is too complicated or you edit the ISA to include too many instructions you will encounter a [Combinatorial explosion](https://en.wikipedia.org/wiki/Combinatorial_explosion) and the program may not exit before the heat death of the universe. To try and avoid this situation I've added the `ISASubset` class which allows only a subset of the total ISA to be selected each time.

## Build Instructions

I've included a VS2017 solution and project (albeit with hardcoded paths), but it should be easy to configure for any build system. The only dependency is Z3 (I used the prebuilt Windows binaries) and you just need to build the two .cpp files in the project somehow.

## Contact

You can reach me on either nick dot gildea at gmail dot com or ngildea85 on Twitter.


