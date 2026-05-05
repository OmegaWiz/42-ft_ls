# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: kkaiyawo <kkaiyawo@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2023/02/15 09:04:03 by kkaiyawo          #+#    #+#              #
#    Updated: 2026/05/05 10:41:21 by kkaiyawo         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

SRCS		= src/ft_ls.c \
			src/parse/t_opts.c \
			src/path/t_path.c

OBJS		= $(patsubst src/%.c, obj/%.o, $(SRCS))

CC			= gcc

CFLAGS		= -Wall -Wextra -Werror -g -I include

RM			= rm -f

LIBFT		= include/libft/libft.a

NAME		= ft_ls

obj/%.o:	src/%.c
			@mkdir -p $(dir $@)
			${CC} ${CFLAGS} -c $< -o $@

all:		${NAME}

${NAME}:	${OBJS} ${LIBFT}
			${CC} ${CFLAGS} ${OBJS} ${LIBFT} -o ${NAME}

${LIBFT}:
			make -C include/libft

clean:
			${RM} -r obj

fclean:		clean
			${RM} ${NAME}

re:			fclean ${NAME}

.PHONY:		all clean fclean re

# valgrind --leak-check=full -s ./ft_ls