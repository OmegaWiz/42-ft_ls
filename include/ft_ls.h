/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_ls.h                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: kkaiyawo <kkaiyawo@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/24 17:31:23 by kkaiyawo          #+#    #+#             */
/*   Updated: 2026/05/21 11:27:39 by kkaiyawo         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef FT_LS_H
# define FT_LS_H

// libft, write, readlink, malloc, free, exit
# include "libft/libft.h"
// opendir, readdir, closedir
# include <dirent.h>
// stat, lstat
# include <sys/stat.h>
// getpwuid, getgrgid
# include <pwd.h>
# include <grp.h>
// listxattr, getxattr
# include <sys/xattr.h>
// time, ctime
# include <time.h>
// perror, strerror
# include <stdio.h>
# include <errno.h>

typedef struct s_opts
{
	int	recursive;
	int	all;
	int	long_format;
	int	reverse_order;
	int	time_sort;
}	t_opts;

extern t_opts	g_opts;

int	render_flags(char *arg);

extern t_darr	g_pending_dirs;

typedef struct s_path
{
	char		*name;
	struct stat	s_stat;
	// DIR				*dir;
	// struct dirent	*s_entry;
}	t_path;

void	*init_path();
t_list	*new_path(char *name);
void	del_path(void *content);

#endif
