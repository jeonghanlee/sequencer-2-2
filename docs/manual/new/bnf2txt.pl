BEGIN{
  print ".. productionlist::\n"
}

sub op ($) {
  return [OPERATOR,@_[0]]
}
sub lit ($) {
  return [LITERAL,@_[0]]
}
sub kw ($) {
  return [KEYWORD,@_[0]]
}
sub typ ($) {
  return [TYPEWORD,@_[0]]
}
sub val ($) {
  return [BUILTIN,@_[0]]
}
sub id ($) {
  return [IDENTIFIER,@_[0]]
}
sub del ($) {
  return [DELIMITER,@_[0]]
}

sub pr_tok {
  my ($type,$value) = @_;
  return "\"$value\"" if $type eq OPERATOR;
  return "`$value`" if $type eq LITERAL;
  return "\"$value\"" if $type eq KEYWORD;
  return "\"$value\"" if $type eq TYPEWORD;
  return "\"$value\"" if $type eq BUILTIN;
  return "`$value`"if $type eq IDENTIFIER;
  return "\"$value\"" if $type eq DELIMITER;
}

my %tok = (
  ADD       => op  '+',
  ADDEQ     => op  '+=',
  AMPERSAND => op  '&',
  ANDAND    => op  '&&',
  ANDEQ     => op  '&=',
  ASSIGN    => kw  'assign',
  ASTERISK  => op  '*',
  BREAK     => kw  'break',
  CARET     => op  '^',
  CCODE     => lit 'embedded_c_code',
  CHAR      => typ 'char',
  COLON     => op  ':',
  COMMA     => del ',',
  CONTINUE  => kw  'continue',
  DECLARE   => kw  'declare',
  DECR      => op  '--',
  DELAY     => val 'delay',
  DIVEQ     => op '/=',
  DOUBLE    => typ 'double',
  ELSE      => kw  'else',
  ENTRY     => kw  'entry',
  EQ        => op  '==',
  EQUAL     => op  '=',
  EVFLAG    => typ 'evflag',
  EXIT      => kw  'exit',
  FLOAT     => typ 'float',
  FOR       => kw  'for',
  FPCON     => lit 'floating_point_literal',
  GE        => op  '>=',
  GT        => op  '>',
  IF        => op  'if',
  INCR      => op  '++',
  INT       => op  'int',
  INTCON    => lit 'integer_literal',
  LBRACE    => del '{',
  LBRACKET  => del '[',
  LE        => op  '<=',
  LONG      => typ 'long',
  LPAREN    => del '(',
  LSHEQ     => op  '<<=',
  LSHIFT    => op  '<<',
  LT        => op  '<',
  MOD       => op  '%',
  MODEQ     => op  '%=',
  MONITOR   => kw  'monitor',
  MULEQ     => op  '*=',
  NAME      => id  'identifier',
  NE        => op  '!=',
  NOT       => op  '!',
  OPTION    => kw  'option',
  OREQ      => op  '|=',
  OROR      => op  '||',
  PERIOD    => op  '.',
  POINTER   => op  '->',
  PROGRAM   => kw  'program',
  QUESTION  => op  '?',
  RBRACE    => del '}',
  RBRACKET  => del ']',
  RPAREN    => del ')',
  RSHEQ     => op  '>>=',
  RSHIFT    => op  '>>',
  SEMICOLON => del ';',
  SHORT     => typ 'short',
  SLASH     => op  '/',
  SS        => kw  'ss',
  STATE     => kw  'state',
  STRCON    => lit 'string_literal',
  STRING    => typ 'string',
  SUB       => op  '-',
  SUBEQ     => op  '-=',
  SYNC      => kw  'sync',
  SYNCQ     => kw  'syncQ',
  TILDE     => op  '~',
  TO        => kw  'to',
  UNSIGNED  => typ 'unsigned',
  VBAR      => op  '|',
  WHEN      => kw  'when',
  WHILE     => kw  'while',
  XOREQ     => op  '^=',
);

if (my ($l,$r) = /(\w+) ::= ?(.*)\./) {
  my @rws = split(/ /,$r);
#   if (not $nt{$l}) {
    print "   $l: ";
#     $nt{$l} = 1;
#   } else {
#     my $n = length $l;
#     print ((" " x ($n+3)) . "| ");
#   }
  print join(" ",map(/[a-z_]+/ ? "`$_`" : pr_tok(@{$tok{$_}}), @rws));
  print "\n";
}
