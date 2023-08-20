import json

# Read input JSON file
with open('config.json', 'r') as json_file:
    config_data = json.load(json_file)

# Create output vars.h file
with open('vars.h', 'w') as vars_file:
    vars_file.write("#ifndef VARS_H\n")
    vars_file.write("#define VARS_H\n\n")

    # Iterate through variables in the JSON data
    for key, value in config_data.items():
        # Write #define statement for each variable
        if isinstance(value, (str, int, float, bool)):
            vars_file.write(f"#define {key} {json.dumps(value)}\n")

    vars_file.write("\n#endif // VARS_H\n")

print("vars.h file generated.")