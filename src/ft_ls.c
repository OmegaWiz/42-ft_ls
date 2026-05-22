/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_ls.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kkaiyawo <kkaiyawo@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/24 17:30:55 by kkaiyawo          #+#    #+#             */
/*   Updated: 2026/05/21 21:30:23 by kkaiyawo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/ft_ls.h"

t_opts	g_opts;
t_darr	g_pending_dirs;

#include <stdio.h>

/*
as long as the arguments could be treated as flags, treat them as such
otherwise, treat them as directory/file(s)
NOTE: flags can be combined
NOTE: if no other arguments, `ls` the current dir
*/

/*
flags:
-R Recursively check the directory for subdirectories
-a include hidden files (eg. `.` `..` dotfiles)
-l long format (access ? ? ? ? time)
-r reverse the order of display
-t sort by time modified, (recent first)
*/

/*
combine ALL the directories needed to be listed first,
then sort the order to be displayed (file > folder)
error comes first (2), then actual result (1)
*/

/*
invalid option = 1
*/

/**
 * parse the recieved arguments into options and a list of directories/files to be listed
 */
int	parse(char **argv)
{
	int		on_flag;
	char	*path;

	on_flag = 1;
	g_pending_dirs = ft_darr_init();
	while (*++argv)
	{
		if (on_flag > 0)
			on_flag = render_flags(*argv);
		if (on_flag == EINVAL)
			return (EINVAL);
		if (on_flag == 0)
		{
			path = ft_strdup(*argv);
			if (!path)
				return (ENOMEM);
			ft_darr_push_back(&g_pending_dirs, path);
			g_opts.argc++;
		}
	}
	if (g_pending_dirs.count == 0)
	{
		char *cwd = ft_strdup(".");
		if (!cwd)
			return (ENOMEM);
		ft_darr_push_back(&g_pending_dirs, cwd);
		g_opts.argc = 0;
	}
	return (0);
}

int	recursive_add_path(struct dirent *entry, t_path *pth, DIR *dir, t_darr *files)
{
	if (ft_strncmp(entry->d_name, ".", 2)
		&& ft_strncmp(entry->d_name, "..", 3))
	{
		char *joined = ft_strjoin(pth->name, "/");
		if (!joined)
		{
			free(pth->name);
			free(pth);
			closedir(dir);
			ft_darr_clear(files, del_path);
			return (ENOMEM);
		}
		ft_darr_push_back(&g_pending_dirs, joined);
	}
	return (0);
}

int mn(int a, int b)
{
	return (a < b) ? a : b;
}

char	f_loweri(unsigned int i, char c)
{
	(void) i;
	return (ft_tolower(c));
}

int	cmp_name_utf(const void *a, const void *b)
{
	size_t	i;
	size_t	j;
	char	*na;
	char	*nb;
	int		res;

	na = ft_strmapi(((t_path *) a)->name, f_loweri);
	if (!na)
		return (0);
	nb = ft_strmapi(((t_path *) b)->name, f_loweri);
	if (!nb)
	{
		free(na);
		return (0);
	}
	i = 0;
	j = 0;
	while (na[i] == '.' && na[i] != 0)
		i++;
	while (nb[j] == '.' && nb[j] != 0)
		j++;
	res = ft_strncmp(na + i, nb + j, mn(ft_strlen(na), ft_strlen(nb)) + 1);
	free(na);
	free(nb);
	return (res);
}

int	cmp_time(const void *a, const void *b)
{
	struct timespec ta;
	struct timespec tb;

	ta = ((t_path *) a)->s_stat.st_mtim;
	tb = ((t_path *) b)->s_stat.st_mtim;
	if (ta.tv_sec > tb.tv_sec)
		return (-1);
	if (ta.tv_sec < tb.tv_sec)
		return (1);
	if (ta.tv_nsec > tb.tv_nsec)
		return (-1);
	if (ta.tv_nsec < tb.tv_nsec)
		return (1);
	return (0);
}

/*
choices:
- sort by name, ignore dots, ignore case (default)
- sort by time modified, recent first (-t)
*/
int	sort_path(t_darr *files)
{
	if (g_opts.time_sort == 1)
		ft_darr_sort(files, cmp_time);
	else
		ft_darr_sort(files, cmp_name_utf);
	return (0);
}


int	ft_isspace(char c)
{
	return (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r');
}

void	print_pathname(void *data)
{
	size_t	i;
	int		is_only_spaces;
	t_path	*path;

	is_only_spaces = 1;
	i = 0;
	path = (t_path *)data;
	while (path->name[i])
	{
		if (!ft_isspace(path->name[i]))
			is_only_spaces = 0;
		i++;
	}
	if (is_only_spaces)
	{
		ft_putchar_fd('\'', 1);
		ft_putstr_fd(path->name, 1);
		ft_putendl_fd("\'", 1);
	}
	else
		ft_putendl_fd(path->name, 1);
}

int	print_simple(t_darr *files)
{
	size_t i;

	if (g_opts.reverse_order == 1)
	{
		i = files->count;
		while (i-- > 0)
			print_pathname(files->arr[i]);
	}
	else
	{
		i = 0;
		while (i < files->count)
			print_pathname(files->arr[i++]);
	}
	return (0);
}
int	print_long(t_darr *files)
{
	(void) files;
	return (0);
}

int print_dir(char *path, t_darr *files)
{
	if (!(g_opts.argc == 0 && g_opts.recursive == 0))
		printf("%s:\n", path);
	if (g_opts.long_format == 1)
		print_long(files);
	else
		print_simple(files);
	return (0);
}

int	process_dir(char *path)
{
	t_darr files = ft_darr_init();

	DIR *dir = opendir(path);
	if (!dir)
	{
		ft_putendl_fd("ft_ls: not a directory: ", 2);
		return (errno);
	}
	struct dirent *entry;
	while ((entry = readdir(dir)))
	{
		if (entry->d_name[0] == '.' && g_opts.all == 0)
			continue;

		t_path *pth = malloc(sizeof(t_path));
		if (!pth)
		{
			closedir(dir);
			ft_darr_clear(&files, del_path);
			return (ENOMEM);
		}
		pth->name = ft_strdup(entry->d_name);
		if (!pth->name)
		{
			free(pth);
			closedir(dir);
			ft_darr_clear(&files, del_path);
			return (ENOMEM);
		}
		if (lstat(pth->name, &pth->s_stat) == -1)
		{
			perror("lstat");
			free(pth->name);
			free(pth);
			closedir(dir);
			ft_darr_clear(&files, del_path);
			return (errno);
		}
		ft_darr_push_back(&files, pth);

		if (g_opts.recursive == 1)
		{
			if (recursive_add_path(entry, pth, dir, &files))
				return (ENOMEM);
		}
	}
	closedir(dir);

	sort_path(&files);

	/* print table */
	print_dir(path, &files);
	ft_darr_clear(&files, del_path);
	return (0);
}

int	main(int argc, char **argv)
{
	if (parse(argv))
		return (errno); // TODO: print error
	while (g_pending_dirs.count > 0)
	{
		process_dir((char *)g_pending_dirs.arr[0]);
		ft_darr_remove(&g_pending_dirs, 0, free);
	}
	ft_darr_clear(&g_pending_dirs, free);
	return (argc - argc);
}
