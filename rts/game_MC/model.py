# Copyright (c) 2017-present, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.

import torch
import torch.nn as nn
from collections import Counter

from rlpytorch import Model, ActorCritic

class MiniRTSNet(Model):
    def __init__(self, args):
        # this is the place where you instantiate all your modules
        # you can later access them using the same names you've given them in here
        super(MiniRTSNet, self).__init__(args)
        self._init(args)

    def _init(self, args):
        self.m = args.params["num_unit_type"] + 7
        self.pool = nn.MaxPool2d(2, 2)

        # self.arch = "ccpccp"
        # self.arch = "ccccpccccp"
        if self.args.arch[0] == "\"" and self.args.arch[-1] == "\"":
            self.arch = self.args.arch[1:-1]
        else:
            self.arch = self.args.arch
        self.arch, channels = self.arch.split(";")

        self.num_channels = []
        for v in channels.split(","):
            if v == "-":
                self.num_channels.append(self.m)
            else:
                self.num_channels.append(int(v))

        self.convs = [ nn.Conv2d(self.num_channels[i], self.num_channels[i+1], 3, padding = 1) for i in range(len(self.num_channels)-1) ]
        for i, conv in enumerate(self.convs):
            setattr(self, "conv%d" % (i + 1), conv)

        self.relu = nn.ReLU() if self._no_leaky_relu() else nn.LeakyReLU(0.1)

        if not self._no_bn():
            self.convs_bn = [ nn.BatchNorm2d(conv.out_channels) for conv in self.convs ]
            for i, conv_bn in enumerate(self.convs_bn):
                setattr(self, "conv%d_bn" % (i + 1), conv_bn)

    def _no_bn(self):
        return getattr(self.args, "disable_bn", False)

    def _no_leaky_relu(self):
        return getattr(self.args, "disable_leaky_relu", False)

    def forward(self, input, res):
        # BN and LeakyReLU are from Wendy's code.
        x = input.view(input.size(0), self.m, 20, 20)

        counts = Counter()
        for i in range(len(self.arch)):
            if self.arch[i] == "c":
                c = counts["c"]
                x = self.convs[c](x)
                if not self._no_bn(): x = self.convs_bn[c](x)
                x = self.relu(x)
                counts["c"] += 1
            elif self.arch[i] == "p":
                x = self.pool(x)

        x = x.view(x.size(0), -1)
        return x

class Model_ActorCritic(Model):
    def __init__(self, args):
        super(Model_ActorCritic, self).__init__(args)
        self._init(args)

    def _init(self, args):
        params = args.params
        assert isinstance(params["num_action"], int), "num_action has to be a number. action = " + str(params["num_action"])
        self.params = params
        self.net = MiniRTSNet(args)

        if self.params.get("model_no_spatial", False):
            self.num_unit = params["num_unit_type"]
            linear_in_dim = (params["num_unit_type"] + 7)
        else:
            linear_in_dim = (params["num_unit_type"] + 7) * 25

        self.linear_policy = nn.Linear(linear_in_dim, params["num_action"])
        self.linear_value = nn.Linear(linear_in_dim, 1)
        self.softmax = nn.Softmax()

    def get_define_args():
        return [
            ("arch", "ccpccp;-,64,64,64,-")
        ]

    def forward(self, x):
        if self.params.get("model_no_spatial", False):
            # Replace a complicated network with a simple retraction.
            # Input: batchsize, channel, height, width
            xreduced = x["s"].sum(2).sum(3).squeeze()
            xreduced[:, self.num_unit:] /= 20 * 20
            output = self._var(xreduced)
        else:
            s, res = x["s"], x["res"]
            output = self.net(self._var(s), self._var(res))

        policy = self.softmax(self.linear_policy(output))
        value = self.linear_value(output)
        return value, dict(V=value, pi=policy)

# Format: key, [model, method]
# if method is None, fall back to default mapping from key to method
Models = {
    "actor_critic": [Model_ActorCritic, ActorCritic],
}
