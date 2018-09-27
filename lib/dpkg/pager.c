/*
 * libdpkg - Debian packaging suite library routines
 * pager.c - pager execution support
 *
 * Copyright © 2018 Guillem Jover <guillem@debian.org>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <compat.h>

#include <sys/types.h>

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <dpkg/dpkg.h>
#include <dpkg/i18n.h>
#include <dpkg/string.h>
#include <dpkg/subproc.h>
#include <dpkg/command.h>
#include <dpkg/pager.h>

/**
 * Get a suitable pager.
 *
 * @return A string representing a pager.
 */
const char *
pager_get_exec(void)
{
	const char *pager;

	if (!isatty(0) || !isatty(1))
		return CAT;

	pager = getenv("PAGER");
	if (str_is_unset(pager))
		pager = DEFAULTPAGER;

	return pager;
}

struct pager {
	bool used;
	const char *desc;
	pid_t pid;
	int stdout_old;
	int pipe[2];
};

struct pager *
pager_spawn(const char *desc, const char *filename)
{
	struct pager *pager;

	pager = m_calloc(1, sizeof(*pager));
	pager->used = filename || (isatty(0) && isatty(1));
	pager->desc = desc;

	if (!pager->used)
		return pager;

	m_pipe(pager->pipe);

	pager->pid = subproc_fork();
	if (pager->pid == 0) {
		struct command cmd;
		const char *exec;

		exec = pager_get_exec();

		m_dup2(pager->pipe[0], 0);
		close(pager->pipe[0]);
		close(pager->pipe[1]);

		command_init(&cmd, exec, desc);
		command_add_arg(&cmd, exec);
		command_add_arg(&cmd, filename);
		command_exec(&cmd);
	}

	pager->stdout_old = m_dup(1);
	m_dup2(pager->pipe[1], 1);
	close(pager->pipe[0]);
	close(pager->pipe[1]);

	return pager;
}

void
pager_reap(struct pager *pager)
{
	if (!pager->used)
		return;

	m_dup2(pager->stdout_old, 1);
	subproc_reap(pager->pid, pager->desc, SUBPROC_NOPIPE);

	free(pager);
}