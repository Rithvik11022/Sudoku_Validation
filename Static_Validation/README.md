# Sudoku Validation

## Files
- Assgn1Src-<cs23btech11022>.cpp -> the design code
- inp.txt -> input file 
- Assgn1Readme-<cs23btech11022>.md -> Readme
- Assgn1Report-<cs23btech11022>.pdf -> Report 

## Input format
- The first line contains number of threads and the dimension of the sudoku in that order.
- Next N lines contain the sudoku input.
- Make sure that the input file is named inp.txt

## Output  
- The output is printed in another file in the order : chunk,mixed,sequential.
- The rows/columns checked by the threads are also printed (in case of chunk or mixed)

## Design overview
- Created a class t_data for the purpose of single argument passing in void * functions for thread usage.
- Segregator function for segregating the sudoku into rows,column,grids.
- Created 3 functions each for chunk and mixed to validate rows,columns and grids.
- Created 2 functions to execute the chunk and mixed algorithms.
- Separate function for sequential algorithm. 
- Lastly, main function which takes input from "inp.txt" and executes all the algorithms.

## Compiling and running the program
```
g++ -g <filename>.cpp -o <filename>;./<filename>

```