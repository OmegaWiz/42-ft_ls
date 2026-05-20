/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_ls.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kkaiyawo <kkaiyawo@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/24 17:30:55 by kkaiyawo          #+#    #+#             */
/*   Updated: 2026/05/20 16:38:54 by kkaiyawo         ###   ########.fr       */
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
	/* ensure darr is initialized */
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
		}
	}
	if (g_pending_dirs.count == 0)
	{
		char *cwd = ft_strdup(".");
		if (!cwd)
			return (ENOMEM);
		ft_darr_push_back(&g_pending_dirs, cwd);
	}
	return (0);
}

void	simple_print(void *data)
{
	t_path	*path = (t_path *)data;
	printf("%s\n", path->name);
}

/* sort table by name */
int path_cmp(const void *a, const void *b)
{
	t_path *pa = (t_path *) a;
	t_path *pb = (t_path *) b;
	return (ft_strncmp(pa->name, pb->name, ft_strlen(pa->name) + ft_strlen(pb->name) + 1));
}

int	process_dir(char *path)
{
	t_darr files = ft_darr_init();

	DIR *dir = opendir(path);
	if (!dir)
		return (errno);
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
		ft_darr_push_back(&files, pth);

		if (g_opts.recursive == 1)
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
					ft_darr_clear(&files, del_path);
					return (ENOMEM);
				}
				ft_darr_push_back(&g_pending_dirs, joined);
			}
		}
	}
	closedir(dir);

	printf("%s:\n", path);

	ft_darr_sort(&files, path_cmp);

	/* print table */
	for (size_t i = 0; i < files.count; ++i)
		simple_print(files.arr[i]);
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
