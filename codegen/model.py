# Copyright (c) 2017-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.

from rlpytorch import Model, ActorCritic

import torch
import torch.nn as nn

class Model_ActorCritic(Model):
    def __init__(self, args):
        super(Model_ActorCritic, self).__init__(args)

        params = args.params

        self.linear_dim = 1920
        relu_func = lambda : nn.LeakyReLU(0.1)
        # relu_func = nn.ReLU

        self.trunk = nn.Sequential(
            nn.Conv2d(3 * params["hist_len"], 32, 5, padding = 2),
            relu_func(),
            nn.MaxPool2d(2, 2),
            nn.Conv2d(32, 32, 5, padding = 2),
            relu_func(),
            nn.MaxPool2d(2, 2),
            nn.Conv2d(32, 64, 3, padding = 1),
            relu_func(),
            nn.MaxPool2d(2, 2),
            nn.Conv2d(64, 64, 3, padding = 1),
            relu_func(),
            nn.MaxPool2d(2, 2),
        )

        self.conv2fc = nn.Sequential(
            nn.Linear(self.linear_dim, 512),
            nn.PReLU()
        )

        self.policy_branch = nn.Linear(512, params["num_action"])
        self.value_branch = nn.Linear(512, 1)
        self.softmax = nn.Softmax()

    def forward(self, x):
        print("forwarding")
        # Get the last hist_len frames.
        s1 = self._var(x["mem_in"])
        s2 = self._var(x["mem_out"])
        s3 = self._var(x["code"])
        print(s1)
        print(s2)
        print(s3)
        return None, dict(pi=None, V=0)

# Format: key, [model, method]
Models = {
    "actor_critic" : [Model_ActorCritic, ActorCritic]
}
