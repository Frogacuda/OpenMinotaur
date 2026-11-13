// Recognized but untranslated editor/geometric capabilities fail explicitly.
// All remaining IDs take the original factory's generic Mixer branch.
NeonModule_Base *createModule(int type) {
    if (type == 200 || type == 300 || (type >= 400 && type <= 406) || type == 499)
        return nullptr;
    NeonModule_Base *m;
    if (type == 500)
        m = new showJoy();
    else if (type == 600)
        m = new cellGrid();
    else
        m = new Mixer();
    m->typeId = type;
    return m;
}
