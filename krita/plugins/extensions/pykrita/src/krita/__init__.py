import pykrita
import os
import sys


def pykritaEventHandler(event):
    def _decorator(func):
        setattr(pykrita, event, func)
        del func
    return _decorator

def kDebug(text):
    '''Use KDE way to show debug info

        TODO Add a way to control debug output from partucular plugins (?)
    '''
    plugin = sys._getframe(1).f_globals['__name__']
    pykrita.kDebug('{}: {}'.format(plugin, text))


@pykritaEventHandler('_pluginLoaded')
def on_load(plugin):
    if plugin in init.functions:
        # Call registered init functions for the plugin
        init.fire(plugin=plugin)
        del init.functions[plugin]
    return True


@pykritaEventHandler('_pluginUnloading')
def on_unload(plugin):
    if plugin in unload.functions:
        # Deinitialize plugin
        unload.fire(plugin=plugin)
        del unload.functions[plugin]
    return True


@pykritaEventHandler('_pykritaLoaded')
def on_pykrita_loaded():
    kDebug('PYKRITA LOADED')
    return True


@pykritaEventHandler('_pykritaUnloading')
def on_pykrita_unloading():
    kDebug('UNLOADING PYKRITA')
    return True

