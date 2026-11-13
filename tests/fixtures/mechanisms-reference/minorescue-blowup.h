struct LegacyEntity : entity {
    void oldBlowUp(unsigned seed, int count) {
        explosion *x = new explosion();
        x->count = count;
        x->from = col;
        x->to = col;
        x->from.a = 1.0;
        x->to.a = 0.5;
        x->rot = rot;
        x->seed = seed;
        x->pos = pos;
        x->vel = vel * 0.1;
        x->size = size;
        x->game = game;
        game->addEntity(x, &game->particles);
        dead = true;
    }
};
