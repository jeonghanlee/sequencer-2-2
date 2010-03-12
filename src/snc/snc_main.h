#ifndef INCLsncmainh
#define INCLsncmainh

/* Export various reporting and printing procedures. */

/* append '# <line_num> "<src_file>"\n' to output (if not disabled by cmd-line option) */
void print_line_num(int line_num, char *src_file);

/* Error and warning message support */

/* just the location info */
void report_loc(const char *src_file, int line_num);

/* location plus message */
void report_at(const char *src_file, int line_num, const char *format, ...);

/* with location from this expression */
struct expression;
void report_at_expr(struct expression *ep, const char *format, ...);

/* message only */
void report(const char *format, ...);

#endif	/*INCLsncmainh*/
