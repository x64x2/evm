
    EVM is a stack based virtual machine designed to be suckless and 
    portable alongside compiled binary code.

    COMPILING 
    ----------
    EVM's only "proven" compiler is gcc 4.2.
     It emits output the same as gcc's bytecode with the following changes:

    - Function calls where the return value is unused are followed by a "pop"
      instruction
    - Doubles are typed as 32-bit floats
    - Floats can be loaded as immediates
    - varg offsets are 32-bit
    - Jump tables are in the DATA segment


    libevm is more than just a VM, it also contains an object code processor.  
    Elink doesn't even deal with the VM structures directly, 
    it just uses the library to parse object files, link them, then dump them to a file.
