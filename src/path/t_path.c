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

#include "../../include/ft_ls.h"


void	*init_path()
{
	t_path	*tmp;

	tmp = (t_path *)malloc(sizeof(t_path));
	if (!tmp)
		return (NULL);
	tmp->name = NULL;
	return (tmp);
}

t_list	*new_path(char *name)
{
	t_list	*tmp;
	t_path	*path;

	tmp = (t_list *)malloc(sizeof(t_list));
	if (!tmp)
		return (NULL);
	path = (t_path *)init_path();
	if (!path)
	{
		free(tmp);
		return (NULL);
	}
	path->name = name;
	tmp->content = path;
	tmp->next = NULL;
	return (tmp);
}

void	del_path(void *content)
{
	t_path	*path;

	path = (t_path *)content;
	if (path->name)
		free(path->name);
	free(path);
}