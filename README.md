# lang.python3

** python3 maiken module **

[![Travis](https://travis-ci.org/mkn-mod/lang.python3.svg?branch=master)](https://travis-ci.org/mkn-mod/lang.python3)

Compile/Link phase module

## Prerequisites
  [maiken](https://github.com/Dekken/maiken)

## Usage

```yaml
mod:
- name: lang.python3
  compile:              # calls python3-config --includes
    args:  $str
    with: $module       # e.g. numpy adds includes for module
  link:                 # calls python3-config --ldflags
    args:  $str
    delete: $str1 $str2 # removes strings from python3-config --ldflags
```

## Building

  Windows cl:

    mkn clean build -tSa -EHsc -d


  *nix gcc:

    mkn clean build -tSa "-O2 -fPIC" -d -l "-pthread -ldl"


## Testing

  Windows cl:

    mkn clean build -tSa -EHsc -dp test run

  *nix gcc:

    mkn clean build -tSa "-O2 -fPIC" -dp test -l "-pthread -ldl" run


## Environment Variables

    Key             PYTHON3_HOME
    Type            string
    Default         ""
    Description     If set - looks for python3/config in $PYTHON3_HOME/bin
