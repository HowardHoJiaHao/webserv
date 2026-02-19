# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: Ho Wai Keong <hwai_keo@student.42kl.edu    +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2026/02/18 by Ho Wai Keong             #+#    #+#              #
#    Updated: 2026/02/18 by Ho Wai Keong            ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

# Compiler and flags
CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++11
CPPFLAGS	= -I./include -I./include/config

# Target executable
TARGET		= webserv

# Source files
SRC_DIR		= src
OBJ_DIR		= objs
DEP_DIR		= deps

SRCS		= $(SRC_DIR)/main.cpp \
			  $(SRC_DIR)/dummyConfig/dummyConfig.cpp

OBJS		= $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
DEPS		= $(SRCS:$(SRC_DIR)/%.cpp=$(DEP_DIR)/%.d)

# Colors for output
GREEN		= \033[0;32m
RED			= \033[0;31m
YELLOW		= \033[0;33m
NC			= \033[0m

# Default target
all: $(TARGET)

# Compile executable
$(TARGET): $(OBJS)
	@echo "$(YELLOW)Linking $(TARGET)...$(NC)"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^
	@echo "$(GREEN)✓ Build complete: $(TARGET)$(NC)"

# Object file compilation with automatic dependency generation
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR) $(DEP_DIR)
	@mkdir -p $(dir $@)
	@echo "$(YELLOW)Compiling $<...$(NC)"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MMD -MP -MF $(DEP_DIR)/$*.d -c $< -o $@
	@echo "$(GREEN)✓ Compiled: $@$(NC)"

# Create directories
$(OBJ_DIR) $(DEP_DIR):
	@mkdir -p $@

# Include dependency files
-include $(DEPS)

# Clean object files and dependencies
clean:
	@echo "$(YELLOW)Removing object files...$(NC)"
	@rm -rf $(OBJ_DIR) $(DEP_DIR)
	@echo "$(GREEN)✓ Clean complete$(NC)"

# Full clean (also remove executable)
fclean: clean
	@echo "$(YELLOW)Removing executable...$(NC)"
	@rm -f $(TARGET)
	@echo "$(GREEN)✓ Full clean complete$(NC)"

# Rebuild everything
re: fclean all

# Phony targets
.PHONY: all clean fclean re

# Display help
help:
	@echo "Available targets:"
	@echo "  all    - Build the project (default)"
	@echo "  clean  - Remove object files and dependencies"
	@echo "  fclean - Remove object files, dependencies, and executable"
	@echo "  re     - Rebuild everything"
	@echo "  help   - Display this help message"
