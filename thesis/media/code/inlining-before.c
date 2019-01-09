// int foo(int x) {
//   return x + 1;
// }
FunctionStatement
  Param x
  ReturnStatement
    OperatorStatement: Add
      Variable: x
      Literal: 1

// bar = foo(5)
OperatorStatement: Assn
  Variable: bar
  CallStatement: foo()
    Literal: 5