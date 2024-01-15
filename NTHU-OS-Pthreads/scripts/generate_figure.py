import matplotlib.pyplot as plt
import numpy as np
import sys

def generate_figure(expname, worker, spec):
    out = f'./result/exp{expname[0]}.out'
    # Parse input
    with open(out, 'r') as f:
        lines = f.readlines()
    data = []
    temp = []
    for l in lines:
        d = l[:-1].split(" ") # Extract \n
        if d[0][0] == "T":
            # A new data
            if expname == "1": 
                para = int(d[1]) / 1000000 # To get second
            elif expname == "2-1" or expname == "2-2" or expname == "4" or expname == "5":
                para = (int(d[1]), int(d[2])) 
            else:
                para = int(d[1])
            data.append({
                'data': temp,
                'para': para
            })
            temp = []
        else:
            d = list(map(int, d))
            d[0] /= 1000000 # To get second
            temp.append(d)

    # Specification
    if spec:
        data = [data[i] for i in spec]

    # Start to plot
    params = [d['para'] for d in data]
    axs = []
    legend_x = 1.05
    legend_y = 0.5

    plt.style.use('ggplot')
    _, ax1 = plt.subplots()
    ax1.set_xlabel('time (s)')
    ax1.set_ylabel('# of consumers')
    for _data in data:  
        d = _data['data']
        d = np.array(d)
        axs.append(ax1.plot(d[:, 0], d[:, 1])[0])

    if worker == "y":
        legend_x = 1.15
        ax2=ax1.twinx()
        ax2.set_ylabel('percentage of worker queue size (%)')
        for i, _data in enumerate(data):  
            d = _data['data']
            d = np.array(d)
            if expname == "3":
                ax2.plot(d[:, 0], d[:, 2]/params[i]*100, alpha=0.2)
                axs.append(ax2.fill_between(d[:, 0], d[:, 2]/params[i]*100, 0, alpha=0.1))
            else:
                ax2.plot(d[:, 0], d[:, 2]/200*100, alpha=0.2)
                axs.append(ax2.fill_between(d[:, 0], d[:, 2]/200*100, 0, alpha=0.1))
        
        ax2.grid(False)
    
    params_size = len(params)
    for i in range(params_size):
        params.append(f'{params[i]} percentage')

    plt.legend(axs, params, loc='center left', bbox_to_anchor=(legend_x, legend_y))
    plt.title(f'Exp{expname}')
    plt.savefig(f'./result/img/exp{expname}.png', bbox_inches='tight')

if __name__ == '__main__':
    # Parsing args
    args = sys.argv[1:]
    expname = args[0] if len(args) >= 1 else 1
    worker = args[1] if len(args) >= 2 else "n"
    spec = list(map(int, args[2:])) if len(args) > 2 else []
    # Call it XD
    generate_figure(expname, worker, spec)
