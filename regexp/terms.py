from tools import make_term

# Terms
#

# scan result terms
Int = make_term("Int")
Op = make_term("Op")

# AST terms
Event = make_term("Event")
Choice = make_term("Choice")
Plus = make_term("Plus")
Star = make_term("Star")
Maybe = make_term("Maybe")
Any = make_term("Any")
Concat = make_term("Concat")

# Target terms
Name = make_term("Name")
Screen = make_term("Screen")
Next = make_term("Next")
Split = make_term("Split")
Choice = make_term("Choice")
Jump = make_term("Jump")
Plus = make_term("Plus")
Label = make_term("Label")
