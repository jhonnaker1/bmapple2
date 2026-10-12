/*
 * posix_extra.c - POSIX functions XRoar uses that circle-newlib lacks.
 *
 * regcomp(), regexec(), regfree(): XRoar's configuration parser splits
 * lines with a few small extended regular expressions ("[ \t]+",
 * "([ \t]*=[ \t]*|[ \t]+)", "[ \t]*,[ \t]*", "[ \t]*$" and single
 * characters). This is a small POSIX ERE matcher for that: literals, ".",
 * bracket expressions, groups with "|", "*", "+", "?", "{m,n}", "^" and
 * "$", finding the leftmost-longest match as POSIX does. No REG_ICASE,
 * REG_NEWLINE or back references, and only the whole match is reported.
 *
 * basename(), access(): simple versions on top of stat().
 *
 * The host simulator builds this file too, with the regex functions renamed
 * (see tools/hostsim/Makefile), so it is tested on every run.
 *
 * This file is part of BMCoco, a bare metal Tandy Color Computer emulator for
 * the Raspberry Pi. See LICENSE for licensing details.
 */

#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---- Compiled form ---------------------------------------------------------- */

enum { N_CHAR, N_ANY, N_CLASS, N_GROUP, N_BOL, N_EOL };

struct node;

struct seq {
	struct node *nodes;
	int n;
};

struct node {
	int type;
	int min, max;            /* repetitions; max -1: unlimited */
	unsigned char c;         /* N_CHAR */
	unsigned char set[32];   /* N_CLASS: bit per character */
	struct seq *alts;        /* N_GROUP: alternatives */
	int nalts;
};

struct compiled {
	struct seq *alts;
	int nalts;
};

/* ---- Parser ------------------------------------------------------------------- */

struct parser {
	const char *p;
	int error;
};

static void free_seq(struct seq *s);

static void free_alts(struct seq *alts, int n)
{
	int i;
	for (i = 0; i < n; i++)
		free_seq(&alts[i]);
	free(alts);
}

static void free_seq(struct seq *s)
{
	int i;
	for (i = 0; i < s->n; i++)
		if (s->nodes[i].type == N_GROUP)
			free_alts(s->nodes[i].alts, s->nodes[i].nalts);
	free(s->nodes);
	s->nodes = NULL;
	s->n = 0;
}

static struct node *add_node(struct seq *s)
{
	struct node *bigger = realloc(s->nodes, (s->n + 1) * sizeof(*s->nodes));
	if (bigger == NULL)
		return NULL;
	s->nodes = bigger;
	memset(&s->nodes[s->n], 0, sizeof(*s->nodes));
	s->nodes[s->n].min = s->nodes[s->n].max = 1;
	return &s->nodes[s->n++];
}

static void set_bit(unsigned char *set, unsigned c)
{
	set[c >> 3] |= (unsigned char) (1 << (c & 7));
}

static int parse_class(struct parser *ps, struct node *nd)
{
	int negate = 0, first = 1, i;
	nd->type = N_CLASS;
	if (*ps->p == '^') {
		negate = 1;
		ps->p++;
	}
	while (*ps->p != '\0' && (first || *ps->p != ']')) {
		unsigned lo = (unsigned char) *ps->p++, hi = lo, c;
		first = 0;
		if (*ps->p == '-' && ps->p[1] != ']' && ps->p[1] != '\0') {
			hi = (unsigned char) ps->p[1];
			ps->p += 2;
		}
		for (c = lo; c <= hi; c++)
			set_bit(nd->set, c);
	}
	if (*ps->p != ']')
		return -1;
	ps->p++;
	if (negate)
		for (i = 0; i < 32; i++)
			nd->set[i] = (unsigned char) ~nd->set[i];
	return 0;
}

static int parse_alts(struct parser *ps, struct seq **alts, int *nalts, int in_group);

static int parse_seq(struct parser *ps, struct seq *s, int in_group)
{
	while (*ps->p != '\0' && *ps->p != '|' && !(in_group && *ps->p == ')')) {
		char c = *ps->p++;
		struct node *nd;
		if (c == '*' || c == '+' || c == '?' || c == '{') {
			/* A repetition of the last node. */
			struct node *last = s->n > 0 ? &s->nodes[s->n - 1] : NULL;
			if (last == NULL || last->type == N_BOL || last->type == N_EOL)
				return -1;
			if (c == '*') {
				last->min = 0;
				last->max = -1;
			} else if (c == '+') {
				last->max = -1;
			} else if (c == '?') {
				last->min = 0;
			} else {
				char *end;
				long m = strtol(ps->p, &end, 10), n = m;
				if (end == ps->p)
					return -1;
				if (*end == ',') {
					const char *q = end + 1;
					n = *q == '}' ? -1 : strtol(q, &end, 10);
					if (n >= 0 && end == q)
						return -1;
					if (n < 0)
						end = (char *) q;
				}
				if (*end != '}')
					return -1;
				ps->p = end + 1;
				last->min = (int) m;
				last->max = (int) n;
			}
			continue;
		}
		nd = add_node(s);
		if (nd == NULL)
			return -1;
		switch (c) {
		case '.':
			nd->type = N_ANY;
			break;
		case '^':
			nd->type = N_BOL;
			break;
		case '$':
			nd->type = N_EOL;
			break;
		case '[':
			if (parse_class(ps, nd) < 0)
				return -1;
			break;
		case '(':
			nd->type = N_GROUP;
			if (parse_alts(ps, &nd->alts, &nd->nalts, 1) < 0 || *ps->p != ')')
				return -1;
			ps->p++;
			break;
		case '\\':
			if (*ps->p == '\0')
				return -1;
			c = *ps->p++;
			nd->type = N_CHAR;
			nd->c = (unsigned char) (c == 't' ? '\t' : c == 'n' ? '\n' : c);
			break;
		default:
			nd->type = N_CHAR;
			nd->c = (unsigned char) c;
			break;
		}
	}
	return 0;
}

static int parse_alts(struct parser *ps, struct seq **alts, int *nalts, int in_group)
{
	*alts = NULL;
	*nalts = 0;
	for (;;) {
		struct seq *bigger = realloc(*alts, (*nalts + 1) * sizeof(**alts));
		if (bigger == NULL)
			return -1;
		*alts = bigger;
		(*alts)[*nalts].nodes = NULL;
		(*alts)[*nalts].n = 0;
		(*nalts)++;
		if (parse_seq(ps, &(*alts)[*nalts - 1], in_group) < 0)
			return -1;
		if (*ps->p != '|')
			return 0;
		ps->p++;
	}
}

/* ---- Matcher --------------------------------------------------------------------- */

/* Sets of positions in the subject: one byte per position 0..len. */
struct matcher {
	const char *s;
	size_t len;
};

static int seq_ends(const struct matcher *m, const struct seq *s, const char *from, char *to);

static int alts_ends(const struct matcher *m, const struct seq *alts, int n,
                     const char *from, char *to)
{
	int i;
	char *tmp = calloc(m->len + 1, 1);
	if (tmp == NULL)
		return -1;
	memset(to, 0, m->len + 1);
	for (i = 0; i < n; i++) {
		size_t p;
		if (seq_ends(m, &alts[i], from, tmp) < 0) {
			free(tmp);
			return -1;
		}
		for (p = 0; p <= m->len; p++)
			to[p] |= tmp[p];
	}
	free(tmp);
	return 0;
}

/* One occurrence of ND from each position in FROM; the ends go in TO. */
static int node_once(const struct matcher *m, const struct node *nd, const char *from, char *to)
{
	size_t p;
	if (nd->type == N_GROUP)
		return alts_ends(m, nd->alts, nd->nalts, from, to);
	memset(to, 0, m->len + 1);
	for (p = 0; p <= m->len; p++) {
		unsigned c;
		if (!from[p])
			continue;
		switch (nd->type) {
		case N_BOL:
			if (p == 0)
				to[p] = 1;
			break;
		case N_EOL:
			if (p == m->len)
				to[p] = 1;
			break;
		default:
			if (p == m->len)
				break;
			c = (unsigned char) m->s[p];
			if (nd->type == N_ANY
			    || (nd->type == N_CHAR && c == nd->c)
			    || (nd->type == N_CLASS && (nd->set[c >> 3] & (1 << (c & 7)))))
				to[p + 1] = 1;
			break;
		}
	}
	return 0;
}

static int node_ends(const struct matcher *m, const struct node *nd, const char *from, char *to)
{
	size_t size = m->len + 1, p;
	char *cur = malloc(size), *next = malloc(size);
	int count, changed;
	if (cur == NULL || next == NULL) {
		free(cur);
		free(next);
		return -1;
	}
	memcpy(cur, from, size);
	memset(to, 0, size);
	if (nd->min == 0)
		memcpy(to, from, size);
	for (count = 1; nd->max < 0 || count <= nd->max; count++) {
		if (node_once(m, nd, cur, next) < 0) {
			free(cur);
			free(next);
			return -1;
		}
		changed = 0;
		for (p = 0; p < size; p++) {
			if (next[p] && count >= nd->min && !to[p]) {
				to[p] = 1;
				changed = 1;
			}
		}
		/* Unlimited repetition: stop once nothing new is reached. */
		if (memcmp(cur, next, size) == 0 || (count >= nd->min && !changed && nd->max < 0))
			break;
		memcpy(cur, next, size);
	}
	free(cur);
	free(next);
	return 0;
}

static int seq_ends(const struct matcher *m, const struct seq *s, const char *from, char *to)
{
	size_t size = m->len + 1;
	char *cur = malloc(size);
	int i;
	if (cur == NULL)
		return -1;
	memcpy(cur, from, size);
	for (i = 0; i < s->n; i++) {
		if (node_ends(m, &s->nodes[i], cur, to) < 0) {
			free(cur);
			return -1;
		}
		memcpy(cur, to, size);
	}
	memcpy(to, cur, size);
	free(cur);
	return 0;
}

/* ---- The POSIX interface ---------------------------------------------------- */

/* The compiled pattern is kept at the start of the caller's regex_t, which
   is bigger than a pointer in every C library (newlib, BSD, glibc), so this
   doesn't depend on how a library lays regex_t out. */
typedef char regex_t_holds_a_pointer[sizeof(regex_t) >= sizeof(struct compiled *) ? 1 : -1];

static struct compiled *get_compiled(const regex_t *preg)
{
	struct compiled *c;
	memcpy(&c, preg, sizeof(c));
	return c;
}

static void set_compiled(regex_t *preg, struct compiled *c)
{
	memcpy(preg, &c, sizeof(c));
}

int regcomp(regex_t *preg, const char *pattern, int cflags)
{
	struct parser ps = {pattern, 0};
	struct compiled *c = calloc(1, sizeof(*c));
	(void) cflags;
	if (c == NULL)
		return REG_ESPACE;
	if (parse_alts(&ps, &c->alts, &c->nalts, 0) < 0 || *ps.p != '\0') {
		free_alts(c->alts, c->nalts);
		free(c);
		return REG_BADPAT;
	}
	memset(preg, 0, sizeof(*preg));
	set_compiled(preg, c);
	return 0;
}

int regexec(const regex_t *preg, const char *string, size_t nmatch,
            regmatch_t pmatch[], int eflags)
{
	const struct compiled *c = get_compiled(preg);
	struct matcher m;
	size_t start;
	char *from, *to;
	(void) eflags;
	if (c == NULL)
		return REG_BADPAT;
	m.s = string;
	m.len = strlen(string);
	from = calloc(m.len + 1, 1);
	to = calloc(m.len + 1, 1);
	if (from == NULL || to == NULL) {
		free(from);
		free(to);
		return REG_ESPACE;
	}
	for (start = 0; start <= m.len; start++) {
		size_t end;
		memset(from, 0, m.len + 1);
		from[start] = 1;
		if (alts_ends(&m, c->alts, c->nalts, from, to) < 0)
			break;
		for (end = m.len + 1; end-- > start;) {
			if (to[end]) {
				if (nmatch > 0 && pmatch != NULL) {
					size_t i;
					pmatch[0].rm_so = (regoff_t) start;
					pmatch[0].rm_eo = (regoff_t) end;
					for (i = 1; i < nmatch; i++)
						pmatch[i].rm_so = pmatch[i].rm_eo = -1;
				}
				free(from);
				free(to);
				return 0;
			}
		}
	}
	free(from);
	free(to);
	return REG_NOMATCH;
}

void regfree(regex_t *preg)
{
	struct compiled *c = get_compiled(preg);
	if (c != NULL) {
		free_alts(c->alts, c->nalts);
		free(c);
	}
	set_compiled(preg, NULL);
}

/* ---- Files -------------------------------------------------------------------- */

#ifndef BMCOCO_HOST_LIBC

/* The part of PATH after the last '/'. This is libgen.h's basename(),
   which XRoar calls; newlib's string.h declares the GNU one under another
   symbol, so this one is given its symbol name directly. PATH isn't
   changed. */
char *bmcoco_basename(char *path) __asm__("basename");

char *bmcoco_basename(char *path)
{
	char *slash = strrchr(path, '/');
	return slash != NULL ? slash + 1 : path;
}

/* Everything on the SD card can be read and written. */
int access(const char *path, int mode)
{
	struct stat st;
	(void) mode;
	return stat(path, &st);
}

#endif
