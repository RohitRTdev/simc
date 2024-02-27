# Notes on preprocessor (sime)

<ol>
    <li>sime only understands ASCII text encoding</li>
    <li>undef statements called on macros that are not defined are ignored with a warning message</li>
    <li>#if stmt's having non integer/character constants in the expression part will be evaluated to false</li>
    <li>sime doesn't convert NUL character to white space. It leaves it as such</li>
    <li>#line and #error directives are not supported right now</li>
    <li>There are no default defined macros</li>
</ol>

## Additional implementation details
* According to the C standard, macro calls can be continued outside of where they are defined. This feature is not available in sime as of now. Any unterminated macro call will result in an error message.

*  Concatenation operator concatenates all types of tokens, even if the combined token doesn't make any sense (For ex: *##- will be concatenated to *- even though the compiler will flag this as an invalid token).

* Search order for #include files will be "directory where the input file is present", any directories included by the user with the -I option, and the current working directory.

* Within #if expression, if a macro is expanded to produce the defined() operator, sime treats it as a normal token. The C standard leaves the handling of this case upto the implementor.
