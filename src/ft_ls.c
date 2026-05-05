/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_ls.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kkaiyawo <kkaiyawo@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/24 17:30:55 by kkaiyawo          #+#    #+#             */
/*   Updated: 2026/05/05 10:42:08 by kkaiyawo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/ft_ls.h"

t_opts	g_opts;
t_list	*g_pending_dirs = NULL;

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
	t_list	*tmp;

	on_flag = 1;
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
			tmp = ft_lstnew(path);
			if (!tmp)
			{
				free(path);
				return (ENOMEM);
			}
			ft_lstadd_back(&g_pending_dirs, tmp);
		}
	}
	if (!g_pending_dirs)
	{
		g_pending_dirs = ft_lstnew(ft_strdup("."));
		if (!g_pending_dirs)
			return (ENOMEM);
	}
	return (0);
}

void	simple_print(void *data)
{
	t_path	*path = (t_path *)data;
	printf("%s\n", path->name);
}

int	process_dir(char *path)
{
	// create local linked list of files
	t_list	*files = NULL;
	
	// gobble (lstat, etc. based on flag) each file --> ll
	DIR	*dir = opendir(path);
	if (!dir)
		return (errno);
	struct dirent	*entry;
	while ((entry = readdir(dir)))
	{
		// FLAG -a
		if (entry->d_name[0] == '.' && g_opts.all == 0)
			continue;
		
		t_path	*path = malloc(sizeof(t_path));
		if (!path)
		{
			closedir(dir);
			ft_lstclear(&files, del_path);
			return (ENOMEM);
		}
		path->name = ft_strdup(entry->d_name);
		if (!path->name)
		{
			free(path);
			closedir(dir);
			ft_lstclear(&files, del_path);
			return (ENOMEM);
		}
		t_list	*tmp = ft_lstnew(path);
		if (!tmp)
		{
			free(path->name);
			free(path);
			closedir(dir);
			ft_lstclear(&files, del_path);
			return (ENOMEM);
		}
		// do something more here
		ft_lstadd_back(&files, tmp);

		// if -R, add subdirectories to pending dirs
		if (g_opts.recursive == 1)
		{
			//check if ignored
			if (ft_strncmp(entry->d_name, ".", 2) 
				&& ft_strncmp(entry->d_name, "..", 3))
			{
				t_list	*new = ft_lstnew(ft_strjoin(path->name, "/"));
				if (!new)
				{
					free(path->name);
					free(path);
					closedir(dir);
					ft_lstclear(&files, del_path);
					return (ENOMEM);
				}
				ft_lstadd_back(&g_pending_dirs, new);
			}
		}
	}
	closedir(dir);
	// sort table
	ft_lstsort(&files, NULL);
	// print table
	ft_lstiter(files, simple_print);
	ft_lstclear(&files, del_path);
	return (0);
}

int	main(int argc, char **argv)
{
	if (parse(argv))
		return (errno); // TODO: print error
	while (g_pending_dirs)
	{
		process_dir(g_pending_dirs->content);
		t_list	*tmp = g_pending_dirs;
		g_pending_dirs = g_pending_dirs->next;
		ft_lstdelone(tmp, free);
	}
	return (argc - argc);
}
