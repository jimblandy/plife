#include <cstdio>
#include <cctype>

#include <stdexcept>
#include "life.hh"

namespace life {
//---------------------------------------------------------------------------
static void goto_next_line (FILE *input)
{
	int c;
	do c = getc (input); while ((c != EOF) && (c != '\n'));
}
//---------------------------------------------------------------------------
static void skip_punctuation (FILE *input)
{
	int c;
	do {
		c = getc (input);
	} while ((c == ',') || (c == ' ') || (c == '\n') || (c == '[') || (c == ']') || (c == '(') || (c == ')'));
	ungetc (c, input);
}
//---------------------------------------------------------------------------
void field::load_life10X (FILE *input)
{
	int c;
	int x = 0, y = 0, x0 = 0;

	char description[1024];
	char rule_string[32];

	bool need_desc_header = true;

	if (!input)
		return;

	while ((c = getc (input)) != EOF) {
		if (c == '#') {
			switch (getc (input)) {
			case '\n': break;
			case 'P': case'p':
				fscanf (input, "%d%d\n", &x, &y);
				x0 = x;
				break;
			case 'D': case 'd':
				if (need_desc_header)
					printf ("[Description]\n");
				description[0] = 0;
				fscanf (input, "%1023[^\n]s", description);
				printf ("%s\n", description);
				need_desc_header = false;
				goto_next_line (input);
				break;
			case 'N': case 'n':
				set_rule ();
				print_rule ();
				goto_next_line (input);
				break;
			case 'R': case 'r':
				rule_string[0] = 0;
				if (fscanf (input, "%30[^\n]s",  rule_string) > 0)
					set_rule_from_string (rule_string);
				else
					set_rule ();

				print_rule ();
				goto_next_line (input);
				break;
			default:
				goto_next_line (input);
			}
		} else {
			if (isdigit (c) || c == '-') {
				ungetc (c, input);
				load_list (input, x0, y);
			} else
				switch (c) {
				case '\n': x = x0; y++; break;
				case '*': set_cell (x++, y); break;
				case '.': x++; break;
				}
		}
	}

//	if (need_desc_header)
//		printf ("[No description]\n");
}
//---------------------------------------------------------------------------
void field::load_bell (FILE *input)
{
	int c;
	int x = 0, y = 0;
	int x0 = 0, y0 = 0;

	int prefix = 0;
	int real_prefix;

	char description[1024];
	bool need_desc_header = true;

	if (!input)
		return;

	while ((c = getc (input)) != EOF) {
		if (c == '!') {
			if (need_desc_header)
				printf ("[Description]\n");
			description[0] = 0;
			fscanf (input, "%1023[^\n]s", description);
			printf ("%s\n", description);
			need_desc_header = false;
			goto_next_line (input);
			continue;
		}

		real_prefix = (prefix ? prefix : 1);

		switch (c) {
		case 'k': y0 = -real_prefix; break;
		case 'h': x = x0 = -real_prefix; break;
		case '@': y = y0; getc (input); break;

		case '\n': x = x0; y += real_prefix; break;
		case 'o': case 'O': case '*':
			for (int k = 0; k < real_prefix; k++)
				set_cell (x++, y);
			break;

		case '.': x += real_prefix; break;
		}

		if (isdigit (c))
			prefix = prefix * 10 + (c - '0');
		else
			prefix = 0;
	}

//	if (need_desc_header)
//		printf ("[No description]\n");
}
//---------------------------------------------------------------------------
void field::load_rle (FILE *input)
{
	int c;
	int x = 0, y = 0;
	int x0 = 0;

	int prefix = 0;
	int real_prefix = 0;

	char description[1024];
	char rule_string[32];
	bool done = false;

	bool need_desc_header = true;
	bool start_of_line = true;

	while ((c = getc (input)) != EOF && !done) {

		if (start_of_line) switch (c) {
		case 'x':
			ungetc (c, input);

			fscanf (input, "x =%d, y =%d", &x, &y);
			x = -x / 2;
			y = -y / 2;

			x0 = x;

			rule_string[0] = 0;
			if (fscanf (input, ", rule =%30[^\n]s", rule_string) > 0) {
				set_rule_from_string (rule_string);
				print_rule ();
			}

			break;

		case '#':
			switch (getc (input)) {
			case 'O': case 'o':
			case 'C': case 'c':
				if (need_desc_header)
					printf ("[Description]\n");
				description[0] = 0;
				fscanf (input, "%1023[^\n]s", description);
				printf ("%s\n", description);
				need_desc_header = false;
				break;
			}
			goto_next_line (input);
			start_of_line = true;
			continue;
		}

		start_of_line = false;
		real_prefix = (prefix > 0 ? prefix : 1);

		switch (c) {
		case 'o':
			for (int k = 0; k < real_prefix; k++)
				set_cell (x++, y);
			break;
		case 'b': x += real_prefix; break;
		case '$': y += real_prefix; x = x0; break;
		case '!': done = true; break;
		case '\n': start_of_line = true;
		}

		if (isdigit (c))
			prefix = prefix * 10 + (c - '0');
		else
			prefix = 0;
	}

	if (need_desc_header)
		printf ("[Description]\n");
	while ((c = getc (input)) != EOF)
		putchar (c);
}
//---------------------------------------------------------------------------
void field::load_list (FILE *input, int x0, int y0)
{
	int x, y;
	int m, n;

	while (!feof (input)) {
		skip_punctuation (input);
		m = fscanf (input, "%d", &x);
		skip_punctuation (input);
		n = fscanf (input, "%d", &y);

		if (m + n == 2)
			set_cell (x0 + x, y0 + y);
		else
			break;
	}
}
//---------------------------------------------------------------------------
bool field::load (char *file_name)
{
	FILE *input;

	char *format_desc[] = { "Life 1.05/1.06", "Bell", "RLE" };
	int format = 0;
	int c;
	bool status = false;

	if ((input = fopen (file_name, "r"))) {
		c = getc (input);

		if (!ferror (input)) {
			if (c == '#') {
				if (getc (input) == 'C')
					format = 2;
			}

			if (c == '!') format = 1;
			if (c == 'x') format = 2;

			printf ("[File: %s (%s format)]\n", file_name, format_desc[format]);

			fseek (input, 0, SEEK_SET);

			clear ();

			switch (format) {
			case 0: load_life10X (input); break;
			case 1: load_bell (input); break;
			case 2: load_rle (input); break;
			}
			status = true;
		}

		fclose (input);
	}

	if (status == false)
		perror (file_name);

	return status;
}
//---------------------------------------------------------------------------
} // (namespace life)
