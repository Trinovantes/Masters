IfStatement
  Condition
   !\tikz[remember picture]\node [] (cross-a) {};!UnaryOperator: NOT!\tikz[remember picture]\node [] (cross-A) {};!
      Variable: cond
  TrueBranch!\tikz[remember picture]\node [] (swap-a) {};!
    CallStatement: bar()
  FalseBranch!\tikz[remember picture]\node [] (swap-A) {};!
    CallStatement: foo()