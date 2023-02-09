solutions = [
  { "name"        : 'src',
    "url"         : 'ssh://gpro.lge.com/webos-pro/chromium',
    "deps_file"   : 'DEPS',
    "managed"     : False,
    "custom_deps" : {
    },
    "custom_vars" : {
      'checkout_configuration' : 'small',
      'checkout_x64'           : False,
      'checkout_nacl'          : False
    },
  },
]

